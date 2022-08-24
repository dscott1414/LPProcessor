import struct


class BitObject:
    sizeOfInteger = 32
    
    def set(self, bit):
        self.bits[bit >> 5] |= 1 << (bit & (self.sizeOfInteger - 1));
    
    def reset(self, bit):
        self.bits[bit >> 5] &= ~(1 << (bit & (self.sizeOfInteger - 1)));
    
    def is_set(self, bit):
        return (self.bits[bit >> 5] & (1 << (bit & (self.sizeOfInteger - 1)))) != 0;
    
    def __init__(self, rs, storeSize):
        self.byteIndex = self.bitIndex = -1;
        rs.offset += 4 * storeSize
        self.bits = struct.unpack('<' + str(storeSize) + 'i', rs.f.read(4 * storeSize))
