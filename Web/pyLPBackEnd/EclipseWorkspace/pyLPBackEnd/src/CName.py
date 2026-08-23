"""CName.py - Honorific / first / middle / last / suffix name parts.

Overview:
    Mirrors C++ cName. Slot constants HON..ANY identify which part a
    matcher is looking at. print() builds a short or labelled string;
    toJSON() is the web-API shape.

Pipeline position:
    Embedded in CObject when a resolved entity is a person name.
    identifySpeakers (C++ side) is the producer.

Key entry points:
    - hn() - format one non-empty part
    - print() - concatenate all parts
    - __init__(rs) - read nickName + nine strings
    - toJSON() - dict for the LPWeb / pyLP JSON surface
"""
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
    # Return "" if namePart is empty; else "namePartName:namePart "
    # (label omitted when printShort).
    def hn(self,namePartName,namePart,printShort):
        if len(namePart)==0: return ""
        return ("" if (printShort) else (namePartName+":") ) + namePart +" "

    # Concatenate H1..A parts; append [nickName] unless printShort.
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

    # Deserialize nickName (int) plus hon/hon2/hon3/first/middle/middle2/
    # last/suffix/any strings from LPIO `rs`.
    def __init__(self,rs):
        self.nickName = rs.read_integer()
        self.hon = rs.read_string()
        self.hon2 = rs.read_string()
        self.hon3 = rs.read_string()
        self.first = rs.read_string()
        self.middle = rs.read_string()
        self.middle2 = rs.read_string()
        self.last = rs.read_string()
        self.suffix = rs.read_string()
        self.any = rs.read_string()
        
    def write(self,rs):
    def toJSON(self):
        ret = {}
        ret['nickName'] = self.nickName
        ret['hon'] = self.hon
        ret['hon2'] = self.hon2
        ret['hon3'] = self.hon3
        ret['first'] = self.first
        ret['middle'] = self.middle
        ret['middle2'] = self.middle2
        ret['last'] = self.last
        ret['suffix'] = self.suffix
        ret['any'] = self.any
        return ret
