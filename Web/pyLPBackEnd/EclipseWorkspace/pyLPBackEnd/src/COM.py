import struct

class COM:

    def __init__(self, *, rs=None, o=None, s=None):
        if rs is not None:
            self.object, self.salienceFactor = struct.unpack('<2i', rs.f.read(8))
        else:
            self.object = o
            self.salienceFactor = s
