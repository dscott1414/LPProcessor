"""LPIO.py - Little-endian binary reader for LP serialized dumps.

Overview:
    Loads an entire file into a BytesIO and exposes the C++ dump
    primitives: 32-bit int, UTF-16 NUL-terminated string, int16,
    int64, count-prefixed int array, int8. offset tracks bytes
    consumed so at_eof() can compare against the on-disk size.

Pipeline position:
    Front door for every pyLP deserializer (Form, TFI, WordMatch,
    CObject, Relation, Source, …).

Key entry points:
    - read_integer / read_string / read_short / read_long /
      read_int_array / read_byte
    - __init__(path) - slurp file
    - at_eof() / close() / back(offset)

Notes / gotchas:
    read_string indexes ch[0]/ch[1] without checking that read(2)
    returned two bytes — EOF mid-string raises IndexError.
    back() uses SEEK_CUR; a negative seek past 0 is undefined.
    The whole file is held in RAM.
"""
import struct
import os
from pathlib import Path
from io import BytesIO

class LPIO:
    
    f = None
    offset = 0
    maxOffset = 0
    
    # Consume 4 bytes as a native-endian signed int (struct 'i').
    def read_integer(self):
        self.offset += 4
        return struct.unpack('i', self.f.read(4))[0]
    
    # Read UTF-16 code units until a 0x0000 terminator. Does not handle
    # a short read at EOF (ch[0] will IndexError).
    def read_string(self):
        str_in = bytearray(b'')
        while True:
            self.offset += 2
            ch = self.f.read(2)
            if ch[0]==0 and ch[1] == 0:
                break
            str_in.extend(ch)
        return str_in.decode('utf-16')
    
    # Consume 2 bytes as a native-endian signed short.
    def read_short(self):
        self.offset += 2
        return struct.unpack('h', self.f.read(2))[0]
    
    # Consume 8 bytes as a native-endian signed long long.
    def read_long(self):
        self.offset += 8
        return struct.unpack('q', self.f.read(8))[0]
    
    # Count-prefixed little-endian int32 array. Advances offset by
    # 4 + 4*count (count itself already advanced by read_integer).
    def read_int_array(self):
        count = self.read_integer();
        self.offset += count * 4 
        return struct.unpack('<' + str(count) + 'i', self.f.read(count * 4))

    # Consume 1 byte as a signed char.
    def read_byte(self):
        self.offset += 1
        return struct.unpack('b', self.f.read(1))[0]

    # Slurp `path` into a BytesIO. maxOffset is the on-disk size.
    def __init__(self, path):
        self.f = BytesIO(open(path, "rb").read())
        self.offset = 0
        self.maxOffset = Path(path).stat().st_size

    # True when the logical offset has consumed the whole file.
    def at_eof(self):
        return self.offset == self.maxOffset
        
    # Close the BytesIO (the original file is already closed).
    def close(self):
        self.f.close()

    # Rewind `offset` bytes (SEEK_CUR) and decrease the logical offset.
    def back(self, offset):
        self.offset -= offset;
        self.f.seek(-offset,os.SEEK_CUR)