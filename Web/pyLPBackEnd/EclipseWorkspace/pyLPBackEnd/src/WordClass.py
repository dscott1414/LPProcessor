from Form import Form
from TFI import TFI
import struct

class WordClass:
    words = {}

    def __init__(self, rs):
        self.maxWordId = rs.read_integer()
        self.wordIndexes = struct.unpack('<' + str(self.maxWordId) + 'i', rs.f.read(self.maxWordId * 4))
        self.counts = struct.unpack('<' + str(self.maxWordId) + 'i', rs.f.read(self.maxWordId * 4))
        self.numWordForms = rs.read_integer()
        self.wordForms = struct.unpack('<' + str(self.numWordForms << 1) + 'i', rs.f.read(self.numWordForms * 8))

    def read_specific_word_cache(self, rs):
        numForms = rs.read_integer()
        Form.forms = {}
        for I in range(numForms):
            Form.forms[I] = Form(rs)  
        Form.initialize_forms()
        while not rs.at_eof():
            word = rs.read_string()
            self.words[word] = TFI(rs)

    def query(self, temp):
        return 1
