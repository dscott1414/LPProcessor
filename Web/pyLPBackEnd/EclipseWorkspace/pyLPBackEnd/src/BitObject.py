class BitObject:
    sizeOfInteger = 32
    
    def set(self, bit):
        self.bits[bit>>5]|=1<<(bit&(self.sizeOfInteger-1));
    
    def reset(self, bit):
        self.bits[bit>>5]&=~(1<<(bit&(self.sizeOfInteger-1)));
    
    def isSet(self, bit):
        return (self.bits[bit>>5]&(1<<(bit&(self.sizeOfInteger-1))))!=0;
    
    def __init__(self, rs,storeSize):
        self.bits= {}
        for I in range(storeSize):
            self.bits[I] = rs.readInteger();
        self.byteIndex = self.bitIndex = -1;
