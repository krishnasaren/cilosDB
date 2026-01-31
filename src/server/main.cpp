#include "../include/hybriddb.h"
#include <sstream>

namespace hybriddb {

// ============================================================================
// NETWORK MANAGER (C++)
// ============================================================================

NetworkManager::NetworkManager(uint16_t p, QueryEngine* qe, TransactionManager* tm)
    : port(p), running(false), queryEngine(qe), txnManager(tm), connectionCounter(0) {}

NetworkManager::~NetworkManager() {
    stop();
}

void NetworkManager::initializeSocket() {
#ifdef PLATFORM_WINDOWS
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    
    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);
    
    bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(listenSocket, SOMAXCONN);
#else
    listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    
    int opt = 1;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);
    
    bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(listenSocket, SOMAXCONN);
#endif
}

bool NetworkManager::start() {
    try {
        initializeSocket();
        running = true;
        
        std::thread acceptThread(&NetworkManager::acceptLoop, this);
        acceptThread.detach();
        
        return true;
    } catch (...) {
        return false;
    }
}

void NetworkManager::acceptLoop() {
    while (running) {
        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        
#ifdef PLATFORM_WINDOWS
        SOCKET clientSocket = accept(listenSocket, (sockaddr*)&clientAddr, &clientLen);
#else
        int clientSocket = accept(listenSocket, (sockaddr*)&clientAddr, &clientLen);
#endif
        
        if (clientSocket < 0) continue;
        
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
        std::string addr = std::string(clientIP) + ":" + std::to_string(ntohs(clientAddr.sin_port));
        
        uint64_t connId = connectionCounter++;
        
        auto conn = std::make_unique<ClientConnection>(
            clientSocket, addr, connId, queryEngine, txnManager);
        
        std::thread connThread([c = conn.get()]() { c->run(); });
        connThread.detach();
        
        std::lock_guard<std::mutex> lock(mutex);
        connections.push_back(std::move(conn));
    }
}

void NetworkManager::stop() {
    running = false;
    
#ifdef PLATFORM_WINDOWS
    closesocket(listenSocket);
    WSACleanup();
#else
    close(listenSocket);
#endif
    
    std::lock_guard<std::mutex> lock(mutex);
    for (auto& conn : connections) {
        conn->stop();
    }
    connections.clear();
}

size_t NetworkManager::getActiveConnections() const {
    return connections.size();
}

// ============================================================================
// CLIENT CONNECTION (C++)
// ============================================================================

ClientConnection::ClientConnection(int sock, const std::string& addr, uint64_t connId,
                                 QueryEngine* qe, TransactionManager* tm)
    : socket(sock), clientAddr(addr), connectionId(connId), currentTxnId(0),
      queryEngine(qe), txnManager(tm), active(true) {}

ClientConnection::~ClientConnection() {
#ifdef PLATFORM_WINDOWS
    closesocket(socket);
#else
    close(socket);
#endif
}

bool ClientConnection::sendMessage(const Message& msg) {
    auto data = msg.serialize();
    int sent = send(socket, reinterpret_cast<const char*>(data.data()), data.size(), 0);
    return sent > 0;
}

Message ClientConnection::receiveMessage() {
    uint8_t header[5];
    int received = recv(socket, reinterpret_cast<char*>(header), 5, 0);
    
    if (received != 5) {
        Message msg;
        msg.type = MessageType::ERROR;
        return msg;
    }
    
    MessageType type = static_cast<MessageType>(header[0]);
    uint32_t length = *reinterpret_cast<uint32_t*>(&header[1]);
    
    std::vector<uint8_t> payload(length);
    received = recv(socket, reinterpret_cast<char*>(payload.data()), length, 0);
    
    Message msg;
    msg.type = type;
    msg.payload = payload;
    return msg;
}

void ClientConnection::run() {
    std::cout << "Connection [" << connectionId << "] from " << clientAddr << std::endl;
    
    while (active) {
        Message msg = receiveMessage();
        
        switch (msg.type) {
            case MessageType::QUERY: {
                std::string query(msg.payload.begin(), msg.payload.end());
                handleQuery(query);
                break;
            }
            case MessageType::BEGIN_TXN: {
                currentTxnId = txnManager->begin();
                Message response;
                response.type = MessageType::RESULT;
                sendMessage(response);
                break;
            }
            case MessageType::COMMIT_TXN: {
                bool success = txnManager->commit(currentTxnId);
                currentTxnId = 0;
                Message response;
                response.type = success ? MessageType::RESULT : MessageType::ERROR;
                sendMessage(response);
                break;
            }
            case MessageType::ROLLBACK_TXN: {
                txnManager->rollback(currentTxnId);
                currentTxnId = 0;
                Message response;
                response.type = MessageType::RESULT;
                sendMessage(response);
                break;
            }
            case MessageType::DISCONNECT:
                active = false;
                break;
            default:
                break;
        }
    }
    
    std::cout << "Connection [" << connectionId << "] closed" << std::endl;
}

void ClientConnection::handleQuery(const std::string& query) {
    // Simple query parsing (in production, use a proper SQL parser)
    Message response;
    response.type = MessageType::RESULT;
    
    // Parse and execute query
    // For now, just echo back
    response.payload.assign(query.begin(), query.end());
    
    sendMessage(response);
}

void ClientConnection::stop() {
    active = false;
}

std::vector<uint8_t> Message::serialize() const {
    std::vector<uint8_t> buffer;
    buffer.push_back(static_cast<uint8_t>(type));
    
    uint32_t len = payload.size();
    buffer.push_back((len >> 0) & 0xFF);
    buffer.push_back((len >> 8) & 0xFF);
    buffer.push_back((len >> 16) & 0xFF);
    buffer.push_back((len >> 24) & 0xFF);
    
    buffer.insert(buffer.end(), payload.begin(), payload.end());
    return buffer;
}

// ============================================================================
// ADMIN INTERFACE (C++ HTTP Server)
// ============================================================================

AdminInterface::AdminInterface(uint16_t p, Server* srv)
    : port(p), running(false), server(srv) {}

AdminInterface::~AdminInterface() {
    stop();
}

bool AdminInterface::start() {
    try {
#ifdef PLATFORM_WINDOWS
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
        
        listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);
        
        bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
        listen(listenSocket, SOMAXCONN);
#else
        listenSocket = socket(AF_INET, SOCK_STREAM, 0);
        
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);
        
        bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
        listen(listenSocket, SOMAXCONN);
#endif
        
        running = true;
        
        std::thread([this]() {
            while (running) {
                sockaddr_in clientAddr;
                socklen_t clientLen = sizeof(clientAddr);
                int clientSocket = accept(listenSocket, (sockaddr*)&clientAddr, &clientLen);
                
                if (clientSocket > 0) {
                    std::thread([this, clientSocket]() {
                        handleHTTPRequest(clientSocket);
#ifdef PLATFORM_WINDOWS
                        closesocket(clientSocket);
#else
                        close(clientSocket);
#endif
                    }).detach();
                }
            }
        }).detach();
        
        return true;
    } catch (...) {
        return false;
    }
}

void AdminInterface::handleHTTPRequest(int clientSocket) {
    char buffer[4096];
    int received = recv(clientSocket, buffer, sizeof(buffer), 0);
    
    if (received <= 0) return;
    
    std::string request(buffer, received);
    std::string response;
    
    if (request.find("GET /api/stats") != std::string::npos) {
        response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n" + generateStatsJSON();
    } else if (request.find("GET /api/tables") != std::string::npos) {
        response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n" + generateTablesJSON();
    } else {
        response = "HTTP/1.1 404 Not Found\r\n\r\n";
    }
    
    send(clientSocket, response.c_str(), response.length(), 0);
}

std::string AdminInterface::generateStatsJSON() {
    auto stats = server->getStats();
    
    std::ostringstream json;
    json << "{";
    json << "\"totalQueries\":" << stats.totalQueries << ",";
    json << "\"activeConnections\":" << stats.activeConnections << ",";
    json << "\"uptime\":" << stats.uptime << ",";
    json << "\"cacheHitRate\":" << stats.cacheHitRate << ",";
    json << "\"tableCount\":" << stats.tableCount;
    json << "}";
    
    return json.str();
}

void AdminInterface::stop() {
    running = false;
#ifdef PLATFORM_WINDOWS
    closesocket(listenSocket);
#else
    close(listenSocket);
#endif
}

// ============================================================================
// MAIN SERVER (C++)
// ============================================================================

Server::Server(const std::string& dataDir, uint16_t dbPort, uint16_t adminPort)
    : dataDirectory(dataDir), dbPort(dbPort), adminPort(adminPort), running(false),
      totalQueries(0), totalConnections(0) {
    
    // Create directories
#ifdef PLATFORM_WINDOWS
    CreateDirectoryA((dataDir + "/tables").c_str(), NULL);
    CreateDirectoryA((dataDir + "/wal").c_str(), NULL);
    CreateDirectoryA((dataDir + "/indexes").c_str(), NULL);
    CreateDirectoryA((dataDir + "/metadata").c_str(), NULL);
#else
    mkdir((dataDir + "/tables").c_str(), 0755);
    mkdir((dataDir + "/wal").c_str(), 0755);
    mkdir((dataDir + "/indexes").c_str(), 0755);
    mkdir((dataDir + "/metadata").c_str(), 0755);
#endif
    
    // Initialize components
    storage = std::make_unique<StorageEngine>(dataDir + "/tables");
    wal = std::make_unique<WALManager>(dataDir + "/wal");
    txnManager = std::make_unique<TransactionManager>(wal.get());
    queryEngine = std::make_unique<QueryEngine>(storage.get(), txnManager.get());
    network = std::make_unique<NetworkManager>(dbPort, queryEngine.get(), txnManager.get());
    admin = std::make_unique<AdminInterface>(adminPort, this);
}

Server::~Server() {
    shutdown();
}

bool Server::start() {
    startTime = std::chrono::system_clock::now();
    
    std::cout << "╔══════════════════════════════════════════════════════╗\n";
    std::cout << "║      HybridDB Server v" << DB_VERSION << "                    ║\n";
    std::cout << "╚══════════════════════════════════════════════════════╝\n\n";
    
    std::cout << "Starting database server...\n";
    std::cout << "Database port: " << dbPort << "\n";
    std::cout << "Admin port: " << adminPort << "\n";
    std::cout << "Data directory: " << dataDirectory << "\n\n";
    
    if (!network->start()) {
        std::cerr << "Failed to start network manager\n";
        return false;
    }
    
    if (!admin->start()) {
        std::cerr << "Failed to start admin interface\n";
        return false;
    }
    
    running = true;
    std::cout << "✓ Server is running!\n";
    std::cout << "✓ Ready to accept connections\n";
    std::cout << "✓ Admin interface: http://localhost:" << adminPort << "\n\n";
    
    return true;
}

void Server::stop() {
    running = false;
    network->stop();
    admin->stop();
}

void Server::shutdown() {
    std::cout << "\nShutting down server...\n";
    stop();
    storage->sync();
    wal->flush();
    std::cout << "✓ Server shutdown complete\n";
}

Server::Stats Server::getStats() const {
    Stats stats;
    stats.totalQueries = totalQueries.load();
    stats.totalConnections = totalConnections.load();
    stats.activeConnections = network->getActiveConnections();
    
    auto now = std::chrono::system_clock::now();
    stats.uptime = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
    
    stats.cacheHitRate = storage->getStorage() ? 0.95 : 0.0; // Placeholder
    stats.tableCount = 0; // Get from catalog
    stats.totalRows = 0;
    stats.walSize = 0;
    
    return stats;
}

} // namespace hybriddb

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================

int main(int argc, char* argv[]) {
    std::string dataDir = "./data";
    uint16_t dbPort = 5432;
    uint16_t adminPort = 8080;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-p" && i + 1 < argc) {
            dbPort = std::atoi(argv[++i]);
        } else if (arg == "-a" && i + 1 < argc) {
            adminPort = std::atoi(argv[++i]);
        } else if (arg == "-d" && i + 1 < argc) {
            dataDir = argv[++i];
        }
    }
    
    hybriddb::Server server(dataDir, dbPort, adminPort);
    
    if (!server.start()) {
        std::cerr << "Failed to start server\n";
        return 1;
    }
    
    // Keep running
    std::cout << "Press Ctrl+C to shutdown\n";
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}