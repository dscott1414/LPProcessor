class COM:

    def read(self, b, offset):
        return offset;

    def initialize(self, o, sf):
        self.object = o;
        self.salienceFactor = sf;

    def __init__(self, rs):
        self.object = rs.readInteger();
        self.salienceFactor = rs.readInteger();