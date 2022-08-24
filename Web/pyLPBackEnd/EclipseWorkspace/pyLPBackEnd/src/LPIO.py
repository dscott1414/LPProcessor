import struct
import os
from pathlib import Path
from io import BytesIO

class LPIO:
    
    f = None
    offset = 0
    maxOffset = 0
    
    def read_integer(self):
        self.offset += 4
        return struct.unpack('i', self.f.read(4))[0]
    
    def read_string(self):
        str_in = bytearray(b'')
        while True:
            self.offset += 2
            ch = self.f.read(2)
            if ch[0]==0 and ch[1] == 0:
                break
            str_in.extend(ch)
        return str_in.decode('utf-16')
    
    def read_short(self):
        self.offset += 2
        return struct.unpack('h', self.f.read(2))[0]
    
    def read_long(self):
        self.offset += 8
        return struct.unpack('q', self.f.read(8))[0]
    
    def read_int_array(self):
        count = self.read_integer();
        self.offset += count * 4 
        return struct.unpack('<' + str(count) + 'i', self.f.read(count * 4))

    def read_byte(self):
        self.offset += 1
        return struct.unpack('b', self.f.read(1))[0]

    def __init__(self, path):
        self.f = BytesIO(open(path, "rb").read())
        self.offset = 0
        self.maxOffset = Path(path).stat().st_size

    def at_eof(self):
        return self.offset == self.maxOffset
        
    def close(self):
        self.f.close()

    def back(self, offset):
        self.offset -= offset;
        self.f.seek(-offset,os.SEEK_CUR)