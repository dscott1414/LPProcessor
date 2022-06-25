from Form import Form
from TFI import TFI
from LPIO import LPIO 

class WordClass:
    maxWordId = 0
    counts = {}
    numWordForms = 0
    wordForms = {}
    words = {}
    wordIndexes = {}

    def __init__(self, rs):
        self.maxWordId = rs.readInteger()
        for I in range(self.maxWordId):
            self.wordIndexes[I] = rs.readInteger()
        for I in range(self.maxWordId):
            self.counts[I] = rs.readInteger()
        self.numWordForms = rs.readInteger()
        for I in range(self.numWordForms << 1):
            self.wordForms[I] = rs.readInteger()

    def readSpecificWordCache(self, rs):
        numForms = rs.readInteger()
        Form.forms = {}
        for I in range(numForms):
            Form.forms[I] = Form(rs)  
        Form.initializeForms()
        while not rs.atEof():
            word = rs.readString()
            self.words[word] = TFI(rs)

    def query(self, temp):
        return 1
