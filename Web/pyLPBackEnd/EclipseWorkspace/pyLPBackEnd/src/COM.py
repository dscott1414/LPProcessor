"""COM.py - (object id, salience) pair from an LP dump.

Overview:
    Eight-byte record used in Section speaker/object lists and
    SpeakerGroup.replacedSpeakers. Mirrors a C++ cOM / similar.
"""
import struct

class COM:

    # Unpack object index and salienceFactor (two int32s).
    def __init__(self, rs):
        self.object, self.salienceFactor = struct.unpack('<2i', rs.f.read(8))
