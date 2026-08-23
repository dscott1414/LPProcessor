"""WordClass.py - Lexicon sidecar: word-id tables plus the TFI cache.

Overview:
    __init__ reads maxWordId, wordIndexes, counts, and packed wordForms
    (id,form pairs). read_specific_word_cache then fills Form.forms and
    the class-level words{string: TFI} map from the rest of the stream.

Pipeline position:
    Loaded before Source so WordMatch can resolve word strings to TFI.

Key entry points:
    - __init__(rs) - id/count/form tables
    - read_specific_word_cache(rs) - Form.forms + words{}
    - query(temp) - stub, always returns 1
"""
from Form import Form
from TFI import TFI
import struct

class WordClass:
    words = {}

    # Read word-id parallel arrays and the packed wordForms table.
    def __init__(self, rs):
        self.maxWordId = rs.read_integer()
        self.wordIndexes = struct.unpack('<' + str(self.maxWordId) + 'i', rs.f.read(self.maxWordId * 4))
        self.counts = struct.unpack('<' + str(self.maxWordId) + 'i', rs.f.read(self.maxWordId * 4))
        self.numWordForms = rs.read_integer()
        self.wordForms = struct.unpack('<' + str(self.numWordForms << 1) + 'i', rs.f.read(self.numWordForms * 8))

    # Replace Form.forms, initialize_forms, then read word/TFI pairs
    # until EOF into WordClass.words.
    def read_specific_word_cache(self, rs):
        numForms = rs.read_integer()
        Form.forms = {}
        for I in range(numForms):
            Form.forms[I] = Form(rs)  
        Form.initialize_forms()
        while not rs.at_eof():
            word = rs.read_string()
            self.words[word] = TFI(rs)

    # Placeholder; always returns 1. `temp` unused.
    def query(self, temp):
        return 1
