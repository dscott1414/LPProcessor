class CName:
    HON=1
    HON2=2
    HON3=3
    FIRST=4
    MIDDLE=5
    MIDDLE2=6
    LAST=7
    SUFFIX=8
    ANY=9
    
    # identifySpeakers
    def hn(self,namePartName,namePart,printShort):
        if len(namePart)==0: return ""
        return ("" if (printShort) else (namePartName+":") ) + namePart +" "

    def print(self,printShort):
        message=""
        message += self.hn("H1",self.hon,printShort)
        message += self.hn("H2",self.hon2,printShort)
        message += self.hn("H3",self.hon3,printShort)
        message += self.hn("F",self.first,printShort)
        message += self.hn("M1",self.middle,printShort)
        message += self.hn("M2",self.middle2,printShort)
        message += self.hn("L",self.last,printShort)
        message += self.hn("S",self.suffix,printShort)
        message += self.hn("A",self.any,printShort)
        if (self.nickName>=0 and not printShort):
            message += "["+str(self.nickName)+"]"
        return message

    def __init__(self,rs):
        self.nickName = rs.readInteger()
        self.hon = rs.readString()
        self.hon2 = rs.readString()
        self.hon3 = rs.readString()
        self.first = rs.readString()
        self.middle = rs.readString()
        self.middle2 = rs.readString()
        self.last = rs.readString()
        self.suffix = rs.readString()
        self.any = rs.readString()