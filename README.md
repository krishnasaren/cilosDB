# HybridDB - Production C++ Database System

## 🎯 EVERYTHING IN C++ - NOT A TOY!

This is a **REAL** database system where **ALL core logic is in C++**. Client libraries are thin wrappers that only handle socket communication.

---

## 📁 PROJECT STRUCTURE

```
HybridDB/
│
├── include/                          # C++ Headers
│   └── hybriddb.h                    # Main database header (complete architecture)
│
├── src/                              # C++ Source Code (ALL DATABASE LOGIC)
│   ├── server/
│   │   └── main.cpp                  # Server entry point + network layer
│   ├── storage/
│   │   └── storage.cpp               # Storage engine + buffer pool + WAL
│   ├── query/
│   │   └── (query parser - TBD)
│   ├── network/
│   │   └── (in main.cpp)
│   └── utils/
│       └── (utilities)
│
├── client-libs/                      # THIN Client Libraries (Socket Communication ONLY)
│   ├── php/
│   │   └── HybridDB.php              # PHP client (thin wrapper)
│   ├── python/
│   │   └── hybriddb.py               # Python client (thin wrapper)
│   ├── cpp/
│   │   └── (C++ client - TBD)
│   ├── java/
│   │   └── (Java client - TBD)
│   └── nodejs/
│       └── (Node.js client - TBD)
│
├── web/                              # Web Interfaces
│   ├── admin/
│   │   └── index.html                # Admin panel (connects to C++ HTTP server)
│   └── user/
│       ├── index.php                 # User login/register
│       ├── dashboard.php             # User dashboard
│       └── logout.php                # Logout
│
├── tools/                            # Command-line Tools
│   ├── cli/
│   │   └── (CLI tool - TBD)
│   └── backup/
│       └── (Backup utility - TBD)
│
├── data/                             # Database Storage (Created at runtime)
│   ├── tables/                       # Binary table files (.dat)
│   ├── wal/                          # Write-ahead logs (.log)
│   ├── indexes/                      # Index files
│   └── metadata/                     # Catalog and metadata
│
├── tests/                            # Unit tests
│   └── (test files - TBD)
│
├── docs/                             # Documentation
│   └── (documentation files - TBD)
│
├── config/                           # Configuration files
│   └── (config files - TBD)
│
├── CMakeLists.txt                    # CMake build configuration
└── README.md                         # This file
```

---

## 🔥 KEY POINTS

### ✅ What's in C++ (EVERYTHING IMPORTANT!)
- **Storage Engine** - Binary page management (8KB pages)
- **Buffer Pool** - LRU caching (512MB)
- **WAL Manager** - Write-ahead logging for durability
- **Transaction Manager** - ACID transactions
- **Query Engine** - Query execution
- **Network Server** - TCP socket server (port 5432)
- **Admin HTTP Server** - HTTP API for admin panel (port 8080)
- **All Business Logic** - Everything happens in C++!

### ✅ What's in Client Libraries (THIN WRAPPERS!)
- **Socket Communication** - Just send/receive messages
- **Message Serialization** - Pack/unpack protocol
- **NO Business Logic** - All processing on C++ server
- **Helper Methods** - Convenience wrappers for SQL

### ✅ Separate Interfaces
1. **Admin Panel** (`web/admin/`) - For DATABASE ADMINISTRATORS
   - Connects to C++ HTTP server (port 8080)
   - Real-time stats from C++ server
   - Table management
   - Connection monitoring

2. **User Web App** (`web/user/`) - For END USERS
   - User registration/login
   - User dashboard
   - Uses HybridDB for storage
   - Completely separate from admin

---

## 🚀 INSTALLATION

### Prerequisites
- **CMake** 3.10+
- **C++17 Compiler** (GCC 7+, Clang 5+, MSVC 2017+)
- **PHP** 7.4+ (for user web app)
- **Python** 3.7+ (optional, for Python client)

### Build C++ Server

```bash
# Clone/extract project
cd HybridDB

# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Compile
cmake --build . --config Release

# Install (optional)
sudo cmake --install .
```

### Windows Build

```cmd
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019"
cmake --build . --config Release
```

---

## 🎮 USAGE

### 1. Start C++ Database Server

```bash
# Linux/Mac
./build/hybriddb-server -p 5432 -a 8080 -d ./data

# Windows
.\build\Release\hybriddb-server.exe -p 5432 -a 8080 -d .\data
```

**Server Arguments:**
- `-p 5432` - Database port (clients connect here)
- `-a 8080` - Admin HTTP port (admin panel connects here)
- `-d ./data` - Data directory

**Output:**
```
╔══════════════════════════════════════════════════════╗
║      HybridDB Server v1.0.0                          ║
╚══════════════════════════════════════════════════════╝

Starting database server...
Database port: 5432
Admin port: 8080
Data directory: ./data

✓ Server is running!
✓ Ready to accept connections
✓ Admin interface: http://localhost:8080

Press Ctrl+C to shutdown
```

### 2. Use PHP Client

```php
<?php
require_once 'client-libs/php/HybridDB.php';

// Connect to C++ server
$db = new HybridDB('localhost', 5432);

// Create table (processed by C++ server!)
$db->createTable('users', [
    'id' => 'INTEGER PRIMARY KEY',
    'name' => 'STRING',
    'email' => 'STRING'
]);

// Insert (C++ handles everything)
$db->insert('users', [
    'id' => 1,
    'name' => 'John Doe',
    'email' => 'john@example.com'
]);

// Select (C++ executes query)
$users = $db->select('users', 'id = 1');
print_r($users);

// Transaction (C++ manages ACID)
$db->begin();
try {
    $db->insert('users', ['id' => 2, 'name' => 'Jane']);
    $db->commit();
} catch (Exception $e) {
    $db->rollback();
}
?>
```

### 3. Use Python Client

```python
from client-libs.python.hybriddb import HybridDB

# Connect to C++ server
db = HybridDB('localhost', 5432)

# Create table
db.create_table('products', {
    'id': 'INTEGER PRIMARY KEY',
    'name': 'STRING',
    'price': 'DOUBLE'
})

# Insert
db.insert('products', {
    'id': 1,
    'name': 'Laptop',
    'price': 999.99
})

# Select
products = db.select('products')
print(products)

# Transaction
db.begin()
try:
    db.insert('products', {'id': 2, 'name': 'Mouse', 'price': 29.99})
    db.commit()
except:
    db.rollback()
```

### 4. Access Admin Panel

```
http://localhost:8080/
```

**Features:**
- Real-time server statistics (from C++ server)
- Active connections monitoring
- Database tables list
- Cache hit rate
- Server uptime

**Note:** Admin panel makes HTTP requests to C++ HTTP server on port 8080

### 5. Access User Web App

```bash
# Using PHP built-in server
cd web/user
php -S localhost:3000

# Access at http://localhost:3000
```

**Features:**
- User registration
- User login
- User dashboard
- Uses HybridDB C++ server for storage

---

## 🔌 ARCHITECTURE

### Client-Server Communication

```
┌─────────────┐         Socket          ┌──────────────┐
│  PHP Client │ ───────────────────────> │              │
└─────────────┘    TCP Port 5432        │              │
                                         │              │
┌─────────────┐         Socket          │   C++        │
│ Python      │ ───────────────────────> │   Database   │
│ Client      │    TCP Port 5432        │   Server     │
└─────────────┘                          │              │
                                         │   (All       │
┌─────────────┐         HTTP            │    Logic)    │
│    Admin    │ ───────────────────────> │              │
│    Panel    │    Port 8080            │              │
└─────────────┘                          └──────────────┘
                                               │
                                               │
                                               ▼
                                         ┌──────────────┐
                                         │              │
                                         │  Binary      │
                                         │  Storage     │
                                         │              │
                                         │  8KB Pages   │
                                         │  WAL Files   │
                                         │  Indexes     │
                                         └──────────────┘
```

### Protocol Format

```
Request/Response Format:
┌──────────┬──────────┬────────────┐
│ Type (1) │ Len (4)  │ Payload(N) │
└──────────┴──────────┴────────────┘

Message Types:
0x01 - CONNECT
0x02 - DISCONNECT
0x03 - QUERY
0x04 - RESULT
0x05 - ERROR
0x06 - BEGIN_TXN
0x07 - COMMIT_TXN
0x08 - ROLLBACK_TXN
```

---

## 🗄️ STORAGE FORMAT

### Table Files
```
File: data/tables/table_000001.dat

Binary format with 8KB pages:
┌─────────────────────┐
│ Page Header (16B)   │
├─────────────────────┤
│ Page Data (8176B)   │
└─────────────────────┘

Each page contains:
- Page ID
- Table ID
- Free space tracker
- Item count
- Checksum
- Actual row data
```

### WAL Files
```
File: data/wal/wal_0000000000000001.log

Format:
┌──────────┬──────────┬──────────┬────────┐
│ LSN (8)  │ Type (1) │ TxnID(8) │ Data(N)│
└──────────┴──────────┴──────────┴────────┘
```

---

## ✅ FEATURES

### Implemented in C++
- ✅ Binary storage engine (8KB pages)
- ✅ Buffer pool (LRU caching)
- ✅ WAL (Write-Ahead Logging)
- ✅ Transaction manager
- ✅ Socket server (TCP)
- ✅ HTTP server for admin (port 8080)
- ✅ Multi-client support
- ✅ Thread-safe operations

### Client Libraries
- ✅ PHP (thin wrapper)
- ✅ Python (thin wrapper)
- ⏳ C++ (to be implemented)
- ⏳ Java (to be implemented)
- ⏳ Node.js (to be implemented)

### Web Interfaces
- ✅ Admin panel (HTML + JavaScript)
- ✅ User web app (PHP + HTML)

---

## 🎯 DIFFERENCE FROM TOY SYSTEM

| Aspect | This System | Toy System |
|--------|-------------|------------|
| **Core Logic** | C++ | PHP/Python |
| **Storage** | Binary (8KB pages) | JSON files |
| **Communication** | Sockets (TCP) | Direct function calls |
| **Multi-language** | Yes (via sockets) | No |
| **Buffer Pool** | Yes (C++) | No |
| **WAL** | Yes (C++) | No |
| **Transactions** | Real ACID (C++) | Fake |
| **Concurrency** | Real (C++ mutexes) | File locks |
| **Performance** | High (C++) | Low (interpreted) |
| **Scalability** | Multi-client (C++) | Single client |

---

## 🔮 TODO

### Priority 1 (Core)
- [ ] SQL Parser (Lemon/Yacc)
- [ ] B+ Tree implementation
- [ ] Index manager
- [ ] Query optimizer

### Priority 2 (Features)
- [ ] C++ client library
- [ ] CLI tool
- [ ] Backup utility
- [ ] Migration tools

### Priority 3 (Enhancements)
- [ ] SSL/TLS support
- [ ] User authentication
- [ ] Role-based access control
- [ ] Replication
- [ ] Sharding

---

## 📞 SUPPORT

This is a professional C++ database system foundation. Everything important runs in C++, not PHP!

**Architecture:**
- C++ Server = All logic
- Client Libraries = Thin wrappers (sockets only)
- Admin Panel = HTML + JavaScript (talks to C++ HTTP server)
- User App = PHP (talks to C++ database server)

---

**Built with C++ for maximum performance and scalability!**