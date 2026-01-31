"""
HybridDB Python Client Library

Thin client - ALL database logic in C++ server
This only handles socket communication
"""

import socket
import struct
import json
from typing import List, Dict, Any, Optional

class HybridDB:
    MSG_CONNECT = 0x01
    MSG_DISCONNECT = 0x02
    MSG_QUERY = 0x03
    MSG_RESULT = 0x04
    MSG_ERROR = 0x05
    MSG_BEGIN_TXN = 0x06
    MSG_COMMIT_TXN = 0x07
    MSG_ROLLBACK_TXN = 0x08
    
    def __init__(self, host='localhost', port=5432):
        self.host = host
        self.port = port
        self.socket = None
        self.connected = False
        self.current_txn = None
        self._connect()
    
    def _connect(self):
        """Connect to C++ database server"""
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.connect((self.host, self.port))
        self.connected = True
    
    def _send_message(self, msg_type: int, payload: bytes = b''):
        """Send message to server"""
        length = len(payload)
        message = struct.pack('!BI', msg_type, length) + payload
        self.socket.sendall(message)
    
    def _receive_message(self) -> tuple:
        """Receive message from server"""
        header = self.socket.recv(5)
        if not header:
            raise ConnectionError("Connection lost")
        
        msg_type, length = struct.unpack('!BI', header)
        payload = self.socket.recv(length)
        
        return msg_type, payload
    
    def query(self, sql: str) -> List[Dict]:
        """Execute SQL query - ALL PROCESSING ON C++ SERVER!"""
        self._send_message(self.MSG_QUERY, sql.encode('utf-8'))
        msg_type, payload = self._receive_message()
        
        if msg_type == self.MSG_ERROR:
            raise Exception(payload.decode('utf-8'))
        
        return json.loads(payload.decode('utf-8'))
    
    def begin(self) -> bool:
        """Begin transaction"""
        self._send_message(self.MSG_BEGIN_TXN)
        msg_type, payload = self._receive_message()
        
        if msg_type == self.MSG_RESULT:
            self.current_txn = json.loads(payload.decode('utf-8'))['txn_id']
            return True
        return False
    
    def commit(self) -> bool:
        """Commit transaction"""
        if not self.current_txn:
            raise Exception("No active transaction")
        
        self._send_message(self.MSG_COMMIT_TXN)
        msg_type, _ = self._receive_message()
        
        self.current_txn = None
        return msg_type == self.MSG_RESULT
    
    def rollback(self) -> bool:
        """Rollback transaction"""
        if not self.current_txn:
            raise Exception("No active transaction")
        
        self._send_message(self.MSG_ROLLBACK_TXN)
        msg_type, _ = self._receive_message()
        
        self.current_txn = None
        return msg_type == self.MSG_RESULT
    
    # Helper methods
    def create_table(self, name: str, columns: Dict[str, str]) -> Any:
        """Create table"""
        cols = ', '.join(f"{k} {v}" for k, v in columns.items())
        return self.query(f"CREATE TABLE {name} ({cols})")
    
    def insert(self, table: str, data: Dict[str, Any]) -> Any:
        """Insert row"""
        cols = ', '.join(data.keys())
        vals = ', '.join(f"'{v}'" if isinstance(v, str) else str(v) for v in data.values())
        return self.query(f"INSERT INTO {table} ({cols}) VALUES ({vals})")
    
    def select(self, table: str, where: Optional[str] = None) -> List[Dict]:
        """Select rows"""
        sql = f"SELECT * FROM {table}"
        if where:
            sql += f" WHERE {where}"
        return self.query(sql)
    
    def update(self, table: str, data: Dict[str, Any], where: str) -> Any:
        """Update rows"""
        set_clause = ', '.join(f"{k} = '{v}'" if isinstance(v, str) else f"{k} = {v}" 
                              for k, v in data.items())
        return self.query(f"UPDATE {table} SET {set_clause} WHERE {where}")
    
    def delete(self, table: str, where: str) -> Any:
        """Delete rows"""
        return self.query(f"DELETE FROM {table} WHERE {where}")
    
    def close(self):
        """Close connection"""
        if self.connected:
            self._send_message(self.MSG_DISCONNECT)
            self.socket.close()
            self.connected = False
    
    def __enter__(self):
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
    
    def __del__(self):
        self.close()