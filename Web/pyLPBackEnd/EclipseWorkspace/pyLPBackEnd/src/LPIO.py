import struct
import os
from pathlib import Path

class LPIO:
    
    f = None
    offset = 0
    maxOffset = 0
    
    def readInteger(self):
        self.offset += 4
        return struct.unpack('i', self.f.read(4))[0]
    
    def readString(self):
        EOF = False
        str_in = bytearray(b'')
        while not EOF:
            self.offset += 2
            ch = self.f.read(2)
            if ch[0]==0 and ch[1] == 0:
                return str_in.decode('utf-16');
            str_in += bytearray(ch)
        return str_in.decode('utf-16')
    
    def readShort(self):
        self.offset += 2
        return struct.unpack('h', self.f.read(2))[0]
    
    def readLong(self):
        self.offset += 8
        return struct.unpack('q', self.f.read(8))[0]
    
    def readIntArray(self):
        count = self.readInteger();
        a = {}
        for I in range(count):
            a[I] = self.readInteger();
        return a;

    def readByte(self):
        self.offset += 1
        return struct.unpack('b', self.f.read(1))[0]

    def __init__(self, path):
        self.f = open(path, "rb")
        self.offset = 0
        self.maxOffset = Path(path).stat().st_size

    def atEof(self):
        return self.offset == self.maxOffset
        
    def close(self):
        self.f.close()
