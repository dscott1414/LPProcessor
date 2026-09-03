"""BitObject.py - Packed 32-bit bitset deserialized from an LP dump.

Overview:
    storeSize ints of little-endian bits. set/reset/is_set use
    bit>>5 as the word index and bit&31 as the bit. __init__ reads
    the packed words (also advances rs.offset by 4*storeSize).

Pipeline position:
    Used for winner-form / flag masks on WordMatch-like records.

Notes / gotchas:
    (fixed) self.bits used to be the raw struct.unpack tuple, so
    set()/reset() (item-assignment into self.bits) raised TypeError
    ('tuple' object does not support item assignment). __init__ now
    wraps the unpacked words in list(...) so self.bits is mutable.
    is_set() worked fine either way (read-only indexing). Neither
    set() nor reset() is currently called anywhere in pyLPBackEnd
    (only is_set() is, from WordMatch.py), so this was latent
    dead-code-until-called rather than an active bug.
"""
import struct


class BitObject:
    sizeOfInteger = 32

    # Set bit `bit` (0-based).
    def set(self, bit):
        self.bits[bit >> 5] |= 1 << (bit & (self.sizeOfInteger - 1));

    # Clear bit `bit`.
    def reset(self, bit):
        self.bits[bit >> 5] &= ~(1 << (bit & (self.sizeOfInteger - 1)));

    # True if bit `bit` is 1.
    def is_set(self, bit):
        return (self.bits[bit >> 5] & (1 << (bit & (self.sizeOfInteger - 1)))) != 0;

    # Read storeSize little-endian int32s into self.bits (a mutable list,
    # so set()/reset() can assign into it).
    def __init__(self, rs, storeSize):
        self.byteIndex = self.bitIndex = -1;
        rs.offset += 4 * storeSize
        self.bits = list(struct.unpack('<' + str(storeSize) + 'i', rs.f.read(4 * storeSize)))
