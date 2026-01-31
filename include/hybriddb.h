#ifndef HYBRIDDB_H
#define HYBRIDDB_H

// Platform detection
#ifdef _WIN32
    #define PLATFORM_WINDOWS
    #include <winsock2.h>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #define PLATFORM_UNIX
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <pthread.h>
#endif

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <queue>
#include <fstream>
#include <iostream>

// Configuration
#define DB_VERSION "1.0.0"
#define PAGE_SIZE 8192
#define DEFAULT_PORT 5432
#define MAX_CONNECTIONS 2000
#define BUFFER_POOL_SIZE_MB 512
#define WAL_SEGMENT_SIZE (16 * 1024 * 1024) // 16MB

namespace hybriddb {

// Forward declarations
class Server;
class StorageEngine;
class BufferPool;
class WALManager;
class TransactionManager;
class QueryEngine;
class NetworkManager;
class AdminInterface;

// ============================================================================
// TYPE SYSTEM
// ============================================================================

enum class DataType : uint8_t {
    TYPE_NULL = 0,
    TYPE_BOOLEAN = 1,
    TYPE_INT8 = 2,
    TYPE_INT16 = 3,
    TYPE_INT32 = 4,
    TYPE_INT64 = 5,
    TYPE_FLOAT = 6,
    TYPE_DOUBLE = 7,
    TYPE_STRING = 8,
    TYPE_BINARY = 9,
    TYPE_TIMESTAMP = 10,
    TYPE_JSON = 11
};

struct Value {
    DataType type;
    union {
        bool boolVal;
        int64_t intVal;
        double doubleVal;
    };
    std::string stringVal;
    std::vector<uint8_t> binaryVal;
    
    Value();
    Value(bool v);
    Value(int64_t v);
    Value(double v);
    Value(const std::string& v);
    Value(const char* v);
    
    std::vector<uint8_t> serialize() const;
    static Value deserialize(const uint8_t* data, size_t& offset);
    
    std::string toString() const;
    bool operator==(const Value& other) const;
};

// ============================================================================
// STORAGE LAYER
// ============================================================================

struct PageHeader {
    uint32_t pageId;
    uint32_t tableId;
    uint16_t freeSpace;
    uint16_t itemCount;
    uint32_t flags;
    uint32_t checksum;
} __attribute__((packed));

struct Page {
    PageHeader header;
    uint8_t data[PAGE_SIZE - sizeof(PageHeader)];
    
    Page();
    void initialize(uint32_t pageId, uint32_t tableId);
    uint32_t calculateChecksum() const;
    bool verify() const;
};

struct Tuple {
    uint64_t rowId;
    uint64_t txnId;
    uint64_t timestamp;
    bool deleted;
    std::map<std::string, Value> columns;
    
    std::vector<uint8_t> serialize() const;
    static Tuple deserialize(const uint8_t* data, size_t length);
};

struct ColumnDef {
    std::string name;
    DataType type;
    bool nullable;
    bool primaryKey;
    bool unique;
    Value defaultValue;
};

struct TableSchema {
    uint32_t tableId;
    std::string tableName;
    std::vector<ColumnDef> columns;
    std::string primaryKeyColumn;
    bool isDocumentMode;
    uint64_t rowCount;
    
    std::vector<uint8_t> serialize() const;
    static TableSchema deserialize(const uint8_t* data, size_t length);
};

class StorageEngine {
private:
    std::string dataDirectory;
    std::unique_ptr<BufferPool> bufferPool;
    std::map<uint32_t, std::fstream> tableFiles;
    std::shared_mutex mutex;
    
public:
    StorageEngine(const std::string& dataDir);
    ~StorageEngine();
    
    bool createTable(uint32_t tableId);
    bool dropTable(uint32_t tableId);
    
    Page* readPage(uint32_t tableId, uint32_t pageId);
    bool writePage(uint32_t tableId, const Page& page);
    uint32_t allocatePage(uint32_t tableId);
    
    bool insertTuple(uint32_t tableId, const Tuple& tuple);
    bool updateTuple(uint32_t tableId, uint64_t rowId, const Tuple& tuple);
    bool deleteTuple(uint32_t tableId, uint64_t rowId);
    std::vector<Tuple> scanTable(uint32_t tableId);
    
    void sync();
    void checkpoint();
};

class BufferPool {
private:
    struct Frame {
        Page page;
        uint32_t tableId;
        uint32_t pageId;
        bool dirty;
        bool pinned;
        uint64_t lastAccess;
    };
    
    std::vector<Frame> frames;
    std::unordered_map<uint64_t, size_t> pageMap;
    std::mutex mutex;
    size_t capacity;
    std::atomic<uint64_t> hits;
    std::atomic<uint64_t> misses;
    
public:
    BufferPool(size_t sizeMB);
    
    Page* getPage(uint32_t tableId, uint32_t pageId);
    void markDirty(uint32_t tableId, uint32_t pageId);
    void flushAll();
    double getHitRate() const;
};

// ============================================================================
// WAL (Write-Ahead Logging)
// ============================================================================

enum class WALRecordType : uint8_t {
    BEGIN_TXN = 1,
    COMMIT_TXN = 2,
    ABORT_TXN = 3,
    INSERT = 4,
    UPDATE = 5,
    DELETE = 6,
    CHECKPOINT = 7
};

struct WALRecord {
    WALRecordType type;
    uint64_t lsn;
    uint64_t txnId;
    uint32_t length;
    std::vector<uint8_t> data;
    
    std::vector<uint8_t> serialize() const;
    static WALRecord deserialize(const uint8_t* data);
};

class WALManager {
private:
    std::string walDirectory;
    std::ofstream currentSegment;
    std::atomic<uint64_t> currentLSN;
    std::mutex mutex;
    std::thread flushThread;
    std::atomic<bool> running;
    
    void flushWorker();
    void openNewSegment();
    
public:
    WALManager(const std::string& walDir);
    ~WALManager();
    
    uint64_t appendRecord(const WALRecord& record);
    void flush();
    void checkpoint(uint64_t checkpointLSN);
    void recover();
    
    uint64_t getCurrentLSN() const { return currentLSN.load(); }
};

// ============================================================================
// TRANSACTION MANAGER
// ============================================================================

enum class IsolationLevel {
    READ_UNCOMMITTED,
    READ_COMMITTED,
    REPEATABLE_READ,
    SERIALIZABLE
};

class TransactionManager {
private:
    struct Transaction {
        uint64_t txnId;
        IsolationLevel isolationLevel;
        uint64_t startLSN;
        uint64_t commitLSN;
        std::vector<std::function<void()>> undoLog;
        bool active;
    };
    
    std::map<uint64_t, Transaction> activeTxns;
    std::atomic<uint64_t> txnCounter;
    std::shared_mutex mutex;
    WALManager* walManager;
    
public:
    TransactionManager(WALManager* wal);
    
    uint64_t begin(IsolationLevel level = IsolationLevel::READ_COMMITTED);
    bool commit(uint64_t txnId);
    bool rollback(uint64_t txnId);
    
    void addUndoAction(uint64_t txnId, std::function<void()> action);
};

// ============================================================================
// QUERY ENGINE
// ============================================================================

class QueryEngine {
private:
    StorageEngine* storage;
    TransactionManager* txnManager;
    std::map<std::string, TableSchema> catalog;
    std::atomic<uint32_t> tableIdCounter;
    std::shared_mutex catalogMutex;
    
public:
    QueryEngine(StorageEngine* se, TransactionManager* tm);
    
    // DDL
    bool createTable(const std::string& name, const std::vector<ColumnDef>& columns, bool docMode);
    bool dropTable(const std::string& name);
    TableSchema* getTableSchema(const std::string& name);
    
    // DML
    bool insert(const std::string& table, const std::map<std::string, Value>& values, uint64_t txnId);
    std::vector<Tuple> select(const std::string& table, std::function<bool(const Tuple&)> filter);
    bool update(const std::string& table, uint64_t rowId, const std::map<std::string, Value>& values, uint64_t txnId);
    bool remove(const std::string& table, uint64_t rowId, uint64_t txnId);
    
    void saveCatalog();
    void loadCatalog();
};

// ============================================================================
// NETWORK LAYER
// ============================================================================

enum class MessageType : uint8_t {
    CONNECT = 0x01,
    DISCONNECT = 0x02,
    QUERY = 0x03,
    RESULT = 0x04,
    ERROR = 0x05,
    BEGIN_TXN = 0x06,
    COMMIT_TXN = 0x07,
    ROLLBACK_TXN = 0x08
};

struct Message {
    MessageType type;
    std::vector<uint8_t> payload;
    
    std::vector<uint8_t> serialize() const;
    static Message deserialize(const uint8_t* data, size_t length);
};

class ClientConnection {
private:
#ifdef PLATFORM_WINDOWS
    SOCKET socket;
#else
    int socket;
#endif
    std::string clientAddr;
    uint64_t connectionId;
    uint64_t currentTxnId;
    QueryEngine* queryEngine;
    TransactionManager* txnManager;
    std::atomic<bool> active;
    
    bool sendMessage(const Message& msg);
    Message receiveMessage();
    void handleQuery(const std::string& query);
    
public:
    ClientConnection(int sock, const std::string& addr, uint64_t connId,
                    QueryEngine* qe, TransactionManager* tm);
    ~ClientConnection();
    
    void run();
    void stop();
};

class NetworkManager {
private:
#ifdef PLATFORM_WINDOWS
    SOCKET listenSocket;
#else
    int listenSocket;
#endif
    uint16_t port;
    std::atomic<bool> running;
    std::vector<std::unique_ptr<ClientConnection>> connections;
    std::vector<std::thread> threads;
    std::mutex mutex;
    std::atomic<uint64_t> connectionCounter;
    
    QueryEngine* queryEngine;
    TransactionManager* txnManager;
    
    void acceptLoop();
    void initializeSocket();
    
public:
    NetworkManager(uint16_t port, QueryEngine* qe, TransactionManager* tm);
    ~NetworkManager();
    
    bool start();
    void stop();
    
    size_t getActiveConnections() const;
};

// ============================================================================
// ADMIN INTERFACE (C++ web server)
// ============================================================================

class AdminInterface {
private:
#ifdef PLATFORM_WINDOWS
    SOCKET listenSocket;
#else
    int listenSocket;
#endif
    uint16_t port;
    std::atomic<bool> running;
    Server* server;
    
    void handleHTTPRequest(int clientSocket);
    std::string generateStatsJSON();
    std::string generateTablesJSON();
    std::string generateConnectionsJSON();
    
public:
    AdminInterface(uint16_t port, Server* srv);
    ~AdminInterface();
    
    bool start();
    void stop();
};

// ============================================================================
// MAIN SERVER
// ============================================================================

class Server {
private:
    std::string dataDirectory;
    uint16_t dbPort;
    uint16_t adminPort;
    
    std::unique_ptr<StorageEngine> storage;
    std::unique_ptr<WALManager> wal;
    std::unique_ptr<TransactionManager> txnManager;
    std::unique_ptr<QueryEngine> queryEngine;
    std::unique_ptr<NetworkManager> network;
    std::unique_ptr<AdminInterface> admin;
    
    std::atomic<bool> running;
    std::atomic<uint64_t> totalQueries;
    std::atomic<uint64_t> totalConnections;
    std::chrono::system_clock::time_point startTime;
    
public:
    Server(const std::string& dataDir, uint16_t dbPort, uint16_t adminPort);
    ~Server();
    
    bool start();
    void stop();
    void shutdown();
    
    // Statistics
    struct Stats {
        uint64_t totalQueries;
        uint64_t totalConnections;
        uint64_t activeConnections;
        uint64_t uptime;
        double cacheHitRate;
        size_t tableCount;
        uint64_t totalRows;
        uint64_t walSize;
    };
    
    Stats getStats() const;
    
    StorageEngine* getStorage() { return storage.get(); }
    QueryEngine* getQueryEngine() { return queryEngine.get(); }
    TransactionManager* getTxnManager() { return txnManager.get(); }
};

} // namespace hybriddb

#endif // HYBRIDDB_H