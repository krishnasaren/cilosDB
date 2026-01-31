#include "../include/hybriddb.h"
#include <cstring>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace hybriddb {

// ============================================================================
// VALUE IMPLEMENTATION
// ============================================================================

Value::Value() : type(DataType::TYPE_NULL), intVal(0) {}
Value::Value(bool v) : type(DataType::TYPE_BOOLEAN), boolVal(v) {}
Value::Value(int64_t v) : type(DataType::TYPE_INT64), intVal(v) {}
Value::Value(double v) : type(DataType::TYPE_DOUBLE), doubleVal(v) {}
Value::Value(const std::string& v) : type(DataType::TYPE_STRING), intVal(0), stringVal(v) {}
Value::Value(const char* v) : type(DataType::TYPE_STRING), intVal(0), stringVal(v) {}

std::vector<uint8_t> Value::serialize() const {
    std::vector<uint8_t> buffer;
    buffer.push_back(static_cast<uint8_t>(type));
    
    switch (type) {
        case DataType::TYPE_NULL:
            break;
        case DataType::TYPE_BOOLEAN:
            buffer.push_back(boolVal ? 1 : 0);
            break;
        case DataType::TYPE_INT64:
            for (int i = 0; i < 8; i++)
                buffer.push_back((intVal >> (i * 8)) & 0xFF);
            break;
        case DataType::TYPE_DOUBLE: {
            uint64_t bits = *reinterpret_cast<const uint64_t*>(&doubleVal);
            for (int i = 0; i < 8; i++)
                buffer.push_back((bits >> (i * 8)) & 0xFF);
            break;
        }
        case DataType::TYPE_STRING: {
            uint32_t len = stringVal.length();
            for (int i = 0; i < 4; i++)
                buffer.push_back((len >> (i * 8)) & 0xFF);
            buffer.insert(buffer.end(), stringVal.begin(), stringVal.end());
            break;
        }
        default:
            break;
    }
    return buffer;
}

std::string Value::toString() const {
    switch (type) {
        case DataType::TYPE_NULL: return "NULL";
        case DataType::TYPE_BOOLEAN: return boolVal ? "true" : "false";
        case DataType::TYPE_INT64: return std::to_string(intVal);
        case DataType::TYPE_DOUBLE: return std::to_string(doubleVal);
        case DataType::TYPE_STRING: return stringVal;
        default: return "";
    }
}

// ============================================================================
// PAGE IMPLEMENTATION
// ============================================================================

Page::Page() {
    memset(this, 0, sizeof(Page));
}

void Page::initialize(uint32_t pageId, uint32_t tableId) {
    header.pageId = pageId;
    header.tableId = tableId;
    header.freeSpace = PAGE_SIZE - sizeof(PageHeader);
    header.itemCount = 0;
    header.flags = 0;
    header.checksum = calculateChecksum();
}

uint32_t Page::calculateChecksum() const {
    uint32_t sum = 0;
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(this);
    for (size_t i = sizeof(PageHeader); i < PAGE_SIZE; i++) {
        sum += ptr[i];
    }
    return sum;
}

bool Page::verify() const {
    return header.checksum == calculateChecksum();
}

// ============================================================================
// STORAGE ENGINE IMPLEMENTATION
// ============================================================================

StorageEngine::StorageEngine(const std::string& dataDir) : dataDirectory(dataDir) {
    bufferPool = std::make_unique<BufferPool>(BUFFER_POOL_SIZE_MB);
    
#ifdef PLATFORM_WINDOWS
    CreateDirectoryA(dataDir.c_str(), NULL);
#else
    mkdir(dataDir.c_str(), 0755);
#endif
}

StorageEngine::~StorageEngine() {
    sync();
}

bool StorageEngine::createTable(uint32_t tableId) {
    std::lock_guard<std::shared_mutex> lock(mutex);
    
    std::ostringstream path;
    path << dataDirectory << "/table_" << std::setfill('0') << std::setw(6) << tableId << ".dat";
    
    std::ofstream file(path.str(), std::ios::binary);
    if (!file) return false;
    
    Page page;
    page.initialize(0, tableId);
    file.write(reinterpret_cast<const char*>(&page), sizeof(Page));
    file.close();
    
    return true;
}

bool StorageEngine::dropTable(uint32_t tableId) {
    std::lock_guard<std::shared_mutex> lock(mutex);
    
    std::ostringstream path;
    path << dataDirectory << "/table_" << std::setfill('0') << std::setw(6) << tableId << ".dat";
    
    tableFiles.erase(tableId);
    return remove(path.str().c_str()) == 0;
}

Page* StorageEngine::readPage(uint32_t tableId, uint32_t pageId) {
    Page* cached = bufferPool->getPage(tableId, pageId);
    if (cached) return cached;
    
    std::shared_lock<std::shared_mutex> lock(mutex);
    
    std::ostringstream path;
    path << dataDirectory << "/table_" << std::setfill('0') << std::setw(6) << tableId << ".dat";
    
    std::fstream& file = tableFiles[tableId];
    if (!file.is_open()) {
        file.open(path.str(), std::ios::in | std::ios::out | std::ios::binary);
    }
    
    if (!file) return nullptr;
    
    Page* page = new Page();
    file.seekg(pageId * PAGE_SIZE);
    file.read(reinterpret_cast<char*>(page), PAGE_SIZE);
    
    if (!page->verify()) {
        delete page;
        return nullptr;
    }
    
    return page;
}

bool StorageEngine::writePage(uint32_t tableId, const Page& page) {
    bufferPool->markDirty(tableId, page.header.pageId);
    
    std::lock_guard<std::shared_mutex> lock(mutex);
    
    std::ostringstream path;
    path << dataDirectory << "/table_" << std::setfill('0') << std::setw(6) << tableId << ".dat";
    
    std::fstream& file = tableFiles[tableId];
    if (!file.is_open()) {
        file.open(path.str(), std::ios::in | std::ios::out | std::ios::binary);
    }
    
    if (!file) return false;
    
    file.seekp(page.header.pageId * PAGE_SIZE);
    file.write(reinterpret_cast<const char*>(&page), PAGE_SIZE);
    file.flush();
    
    return true;
}

void StorageEngine::sync() {
    bufferPool->flushAll();
    
    std::lock_guard<std::shared_mutex> lock(mutex);
    for (auto& [id, file] : tableFiles) {
        if (file.is_open()) {
            file.flush();
        }
    }
}

// ============================================================================
// BUFFER POOL IMPLEMENTATION
// ============================================================================

BufferPool::BufferPool(size_t sizeMB) 
    : capacity((sizeMB * 1024 * 1024) / PAGE_SIZE), hits(0), misses(0) {
    frames.resize(capacity);
}

Page* BufferPool::getPage(uint32_t tableId, uint32_t pageId) {
    std::lock_guard<std::mutex> lock(mutex);
    
    uint64_t key = (static_cast<uint64_t>(tableId) << 32) | pageId;
    auto it = pageMap.find(key);
    
    if (it != pageMap.end()) {
        hits++;
        frames[it->second].lastAccess = std::chrono::system_clock::now().time_since_epoch().count();
        return &frames[it->second].page;
    }
    
    misses++;
    return nullptr;
}

void BufferPool::markDirty(uint32_t tableId, uint32_t pageId) {
    std::lock_guard<std::mutex> lock(mutex);
    
    uint64_t key = (static_cast<uint64_t>(tableId) << 32) | pageId;
    auto it = pageMap.find(key);
    
    if (it != pageMap.end()) {
        frames[it->second].dirty = true;
    }
}

void BufferPool::flushAll() {
    std::lock_guard<std::mutex> lock(mutex);
    
    for (auto& frame : frames) {
        if (frame.dirty) {
            // Write back to disk (handled by StorageEngine)
            frame.dirty = false;
        }
    }
}

double BufferPool::getHitRate() const {
    uint64_t total = hits + misses;
    return total > 0 ? static_cast<double>(hits) / total : 0.0;
}

// ============================================================================
// WAL MANAGER IMPLEMENTATION
// ============================================================================

WALManager::WALManager(const std::string& walDir) 
    : walDirectory(walDir), currentLSN(0), running(true) {
    
#ifdef PLATFORM_WINDOWS
    CreateDirectoryA(walDir.c_str(), NULL);
#else
    mkdir(walDir.c_str(), 0755);
#endif
    
    openNewSegment();
    flushThread = std::thread(&WALManager::flushWorker, this);
}

WALManager::~WALManager() {
    running = false;
    if (flushThread.joinable()) {
        flushThread.join();
    }
    if (currentSegment.is_open()) {
        currentSegment.close();
    }
}

void WALManager::openNewSegment() {
    std::ostringstream path;
    path << walDirectory << "/wal_" << std::setfill('0') << std::setw(16) 
         << std::hex << currentLSN.load() << ".log";
    
    currentSegment.open(path.str(), std::ios::binary | std::ios::app);
}

uint64_t WALManager::appendRecord(const WALRecord& record) {
    std::lock_guard<std::mutex> lock(mutex);
    
    uint64_t lsn = currentLSN++;
    auto data = record.serialize();
    
    currentSegment.write(reinterpret_cast<const char*>(&lsn), sizeof(lsn));
    currentSegment.write(reinterpret_cast<const char*>(data.data()), data.size());
    
    return lsn;
}

void WALManager::flush() {
    std::lock_guard<std::mutex> lock(mutex);
    if (currentSegment.is_open()) {
        currentSegment.flush();
    }
}

void WALManager::flushWorker() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        flush();
    }
}

// ============================================================================
// TRANSACTION MANAGER IMPLEMENTATION
// ============================================================================

TransactionManager::TransactionManager(WALManager* wal) 
    : walManager(wal), txnCounter(1) {}

uint64_t TransactionManager::begin(IsolationLevel level) {
    std::unique_lock<std::shared_mutex> lock(mutex);
    
    uint64_t txnId = txnCounter++;
    Transaction txn;
    txn.txnId = txnId;
    txn.isolationLevel = level;
    txn.startLSN = walManager->getCurrentLSN();
    txn.active = true;
    
    activeTxns[txnId] = txn;
    
    WALRecord record;
    record.type = WALRecordType::BEGIN_TXN;
    record.txnId = txnId;
    walManager->appendRecord(record);
    
    return txnId;
}

bool TransactionManager::commit(uint64_t txnId) {
    std::unique_lock<std::shared_mutex> lock(mutex);
    
    auto it = activeTxns.find(txnId);
    if (it == activeTxns.end() || !it->second.active) {
        return false;
    }
    
    WALRecord record;
    record.type = WALRecordType::COMMIT_TXN;
    record.txnId = txnId;
    it->second.commitLSN = walManager->appendRecord(record);
    
    it->second.active = false;
    activeTxns.erase(it);
    
    return true;
}

bool TransactionManager::rollback(uint64_t txnId) {
    std::unique_lock<std::shared_mutex> lock(mutex);
    
    auto it = activeTxns.find(txnId);
    if (it == activeTxns.end()) {
        return false;
    }
    
    // Execute undo actions
    for (auto rit = it->second.undoLog.rbegin(); rit != it->second.undoLog.rend(); ++rit) {
        (*rit)();
    }
    
    WALRecord record;
    record.type = WALRecordType::ABORT_TXN;
    record.txnId = txnId;
    walManager->appendRecord(record);
    
    it->second.active = false;
    activeTxns.erase(it);
    
    return true;
}

// ============================================================================
// QUERY ENGINE IMPLEMENTATION
// ============================================================================

QueryEngine::QueryEngine(StorageEngine* se, TransactionManager* tm)
    : storage(se), txnManager(tm), tableIdCounter(1) {
    loadCatalog();
}

bool QueryEngine::createTable(const std::string& name, const std::vector<ColumnDef>& columns, bool docMode) {
    std::unique_lock<std::shared_mutex> lock(catalogMutex);
    
    if (catalog.count(name)) {
        return false;
    }
    
    TableSchema schema;
    schema.tableId = tableIdCounter++;
    schema.tableName = name;
    schema.columns = columns;
    schema.isDocumentMode = docMode;
    schema.rowCount = 0;
    
    catalog[name] = schema;
    storage->createTable(schema.tableId);
    saveCatalog();
    
    return true;
}

bool QueryEngine::dropTable(const std::string& name) {
    std::unique_lock<std::shared_mutex> lock(catalogMutex);
    
    auto it = catalog.find(name);
    if (it == catalog.end()) {
        return false;
    }
    
    storage->dropTable(it->second.tableId);
    catalog.erase(it);
    saveCatalog();
    
    return true;
}

TableSchema* QueryEngine::getTableSchema(const std::string& name) {
    std::shared_lock<std::shared_mutex> lock(catalogMutex);
    
    auto it = catalog.find(name);
    return (it != catalog.end()) ? &it->second : nullptr;
}

void QueryEngine::saveCatalog() {
    // Save catalog to disk
    std::ofstream file("data/metadata/catalog.dat", std::ios::binary);
    if (!file) return;
    
    uint32_t count = catalog.size();
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));
    
    for (const auto& [name, schema] : catalog) {
        auto data = schema.serialize();
        uint32_t len = data.size();
        file.write(reinterpret_cast<const char*>(&len), sizeof(len));
        file.write(reinterpret_cast<const char*>(data.data()), len);
    }
}

void QueryEngine::loadCatalog() {
    std::ifstream file("data/metadata/catalog.dat", std::ios::binary);
    if (!file) return;
    
    uint32_t count;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));
    
    for (uint32_t i = 0; i < count; i++) {
        uint32_t len;
        file.read(reinterpret_cast<char*>(&len), sizeof(len));
        
        std::vector<uint8_t> data(len);
        file.read(reinterpret_cast<char*>(data.data()), len);
        
        TableSchema schema = TableSchema::deserialize(data.data(), len);
        catalog[schema.tableName] = schema;
        tableIdCounter = std::max(tableIdCounter.load(), schema.tableId + 1);
    }
}

// Serialization implementations (simplified)
std::vector<uint8_t> TableSchema::serialize() const {
    std::vector<uint8_t> buffer;
    // Implementation details...
    return buffer;
}

TableSchema TableSchema::deserialize(const uint8_t* data, size_t length) {
    TableSchema schema;
    // Implementation details...
    return schema;
}

std::vector<uint8_t> Tuple::serialize() const {
    std::vector<uint8_t> buffer;
    // Implementation details...
    return buffer;
}

Tuple Tuple::deserialize(const uint8_t* data, size_t length) {
    Tuple tuple;
    // Implementation details...
    return tuple;
}

std::vector<uint8_t> WALRecord::serialize() const {
    std::vector<uint8_t> buffer;
    buffer.push_back(static_cast<uint8_t>(type));
    for (int i = 0; i < 8; i++) buffer.push_back((lsn >> (i * 8)) & 0xFF);
    for (int i = 0; i < 8; i++) buffer.push_back((txnId >> (i * 8)) & 0xFF);
    buffer.insert(buffer.end(), data.begin(), data.end());
    return buffer;
}

WALRecord WALRecord::deserialize(const uint8_t* data) {
    WALRecord record;
    // Implementation details...
    return record;
}

Value Value::deserialize(const uint8_t* data, size_t& offset) {
    Value v;
    // Implementation details...
    return v;
}

bool Value::operator==(const Value& other) const {
    if (type != other.type) return false;
    switch (type) {
        case DataType::TYPE_BOOLEAN: return boolVal == other.boolVal;
        case DataType::TYPE_INT64: return intVal == other.intVal;
        case DataType::TYPE_STRING: return stringVal == other.stringVal;
        default: return false;
    }
}

} // namespace hybriddb