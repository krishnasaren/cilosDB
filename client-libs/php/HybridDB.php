<?php
/**
 * HybridDB PHP Client Library
 * 
 * Thin client - ALL database logic in C++ server
 * This only handles socket communication
 */

class HybridDB {
    private $socket;
    private $host;
    private $port;
    private $connected = false;
    private $currentTxn = null;
    
    const MSG_CONNECT = 0x01;
    const MSG_DISCONNECT = 0x02;
    const MSG_QUERY = 0x03;
    const MSG_RESULT = 0x04;
    const MSG_ERROR = 0x05;
    const MSG_BEGIN_TXN = 0x06;
    const MSG_COMMIT_TXN = 0x07;
    const MSG_ROLLBACK_TXN = 0x08;
    
    public function __construct($host = 'localhost', $port = 5432) {
        $this->host = $host;
        $this->port = $port;
        $this->connect();
    }
    
    private function connect() {
        $this->socket = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
        
        if (!$this->socket) {
            throw new Exception("Failed to create socket");
        }
        
        if (!socket_connect($this->socket, $this->host, $this->port)) {
            throw new Exception("Failed to connect to {$this->host}:{$this->port}");
        }
        
        $this->connected = true;
    }
    
    private function sendMessage($type, $payload = '') {
        $length = strlen($payload);
        $message = pack('C', $type) . pack('V', $length) . $payload;
        
        socket_write($this->socket, $message, strlen($message));
    }
    
    private function receiveMessage() {
        $header = socket_read($this->socket, 5);
        if (!$header) {
            throw new Exception("Connection lost");
        }
        
        $data = unpack('Ctype/Vlength', $header);
        $payload = socket_read($this->socket, $data['length']);
        
        return [
            'type' => $data['type'],
            'payload' => $payload
        ];
    }
    
    // Execute SQL query - ALL PROCESSING ON C++ SERVER!
    public function query($sql) {
        $this->sendMessage(self::MSG_QUERY, $sql);
        $response = $this->receiveMessage();
        
        if ($response['type'] == self::MSG_ERROR) {
            throw new Exception($response['payload']);
        }
        
        return json_decode($response['payload'], true);
    }
    
    // Transaction methods
    public function begin() {
        $this->sendMessage(self::MSG_BEGIN_TXN);
        $response = $this->receiveMessage();
        
        if ($response['type'] == self::MSG_RESULT) {
            $this->currentTxn = json_decode($response['payload'], true)['txn_id'];
            return true;
        }
        return false;
    }
    
    public function commit() {
        if (!$this->currentTxn) {
            throw new Exception("No active transaction");
        }
        
        $this->sendMessage(self::MSG_COMMIT_TXN);
        $response = $this->receiveMessage();
        
        $this->currentTxn = null;
        return $response['type'] == self::MSG_RESULT;
    }
    
    public function rollback() {
        if (!$this->currentTxn) {
            throw new Exception("No active transaction");
        }
        
        $this->sendMessage(self::MSG_ROLLBACK_TXN);
        $response = $this->receiveMessage();
        
        $this->currentTxn = null;
        return $response['type'] == self::MSG_RESULT;
    }
    
    // Helper methods
    public function createTable($name, $columns) {
        $cols = implode(', ', array_map(function($col, $type) {
            return "$col $type";
        }, array_keys($columns), $columns));
        
        return $this->query("CREATE TABLE $name ($cols)");
    }
    
    public function insert($table, $data) {
        $cols = implode(', ', array_keys($data));
        $vals = implode(', ', array_map(function($v) {
            return is_string($v) ? "'$v'" : $v;
        }, $data));
        
        return $this->query("INSERT INTO $table ($cols) VALUES ($vals)");
    }
    
    public function select($table, $where = null) {
        $sql = "SELECT * FROM $table";
        if ($where) {
            $sql .= " WHERE $where";
        }
        return $this->query($sql);
    }
    
    public function update($table, $data, $where) {
        $set = implode(', ', array_map(function($k, $v) {
            return "$k = " . (is_string($v) ? "'$v'" : $v);
        }, array_keys($data), $data));
        
        return $this->query("UPDATE $table SET $set WHERE $where");
    }
    
    public function delete($table, $where) {
        return $this->query("DELETE FROM $table WHERE $where");
    }
    
    public function __destruct() {
        if ($this->connected) {
            $this->sendMessage(self::MSG_DISCONNECT);
            socket_close($this->socket);
        }
    }
}
?>