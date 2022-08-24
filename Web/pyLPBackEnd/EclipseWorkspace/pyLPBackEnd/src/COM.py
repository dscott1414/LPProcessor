import struct

class COM:

    def __init__(self, rs):
        self.object, self.salienceFactor = struct.unpack('<2i', rs.f.read(8))
