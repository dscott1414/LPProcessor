"""BitObject.py - Packed 32-bit bitset deserialized from an LP dump.

Overview:
    storeSize ints of little-endian bits. set/reset/is_set use
    bit>>5 as the word index and bit&31 as the bit. __init__ reads
    the packed words (also advances rs.offset by 4*storeSize).

Pipeline position:
    Used for winner-form / flag masks on WordMatch-like records.
"""
import struct


class BitObject:
    sizeOfInteger = 32
    
    # Set bit `bit` (0-based). bits is a struct tuple — this will raise
    # TypeError (tuples are immutable). Likely dead after deserialize.
    def set(self, bit):
        self.bits[bit >> 5] |= 1 << (bit & (self.sizeOfInteger - 1));
    
    # Clear bit `bit`. Same tuple-immutability issue as set().
    def reset(self, bit):
        self.bits[bit >> 5] &= ~(1 << (bit & (self.sizeOfInteger - 1)));
    
    # True if bit `bit` is 1. Safe on the unpacked tuple.
    def is_set(self, bit):
        return (self.bits[bit >> 5] & (1 << (bit & (self.sizeOfInteger - 1)))) != 0;
    
    # Read storeSize little-endian int32s into self.bits.
    def __init__(self, rs, storeSize):
        self.byteIndex = self.bitIndex = -1;
        rs.offset += 4 * storeSize
        self.bits = struct.unpack('<' + str(storeSize) + 'i', rs.f.read(4 * storeSize))
