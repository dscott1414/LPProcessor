"""Form.py - Word-class (POS) record deserialized from an LP binary dump.

Overview:
    Mirrors C++ cForm: name, shortName, inflection class, and boolean
    flags (top-level, ignore, verb, block-proper-noun, …). Forms are
    stored in the class-level list Form.forms; initialize_forms()
    caches the ids of a few frequently used names.

Pipeline position:
    Loaded by pyLP / Source when reading a serialized parse. Used by
    TFI and WordMatch to interpret form indexes.

Key entry points:
    - __init__(rs) - read one form from an LPIO stream
    - find_form(name) - linear search; -1 if missing
    - initialize_forms() - set adverbForm / nounForm / …

Notes / gotchas:
    find_form is O(n) and is only called a handful of times at init.
"""
class Form:

    forms = {}

        # Read one form record from LPIO `rs` (two strings + eight shorts
        # as bools). Does not register into Form.forms; the caller appends.
    def __init__(self, rs):
        self.name = rs.read_string()
        self.shortName = rs.read_string()
        self.inflectionsClass = rs.read_string()
        self.hasInflections = rs.read_short() != 0
        self.properNounSubClass = rs.read_short() != 0
        self.isTopLevel = rs.read_short() != 0
        self.isIgnore = rs.read_short() != 0
        self.verbForm = rs.read_short() != 0
        self.blockProperNounRecognition = rs.read_short() != 0
        self.formCheck = rs.read_short() != 0

    @staticmethod
        # Linear scan of Form.forms for an exact name match.
        # Returns the index, or -1 if not found.
    def find_form(name):
        for f in range(len(Form.forms)):
            if (Form.forms[f].name == name):
                return f
        return -1
    
    @staticmethod
        # Cache indexes of adverb/noun/adjective/preposition/honorific
        # forms. Safe to call before forms are loaded (ids stay -1).
    def initialize_forms():
        Form.adverbForm = Form.find_form("adverb")
        Form.nounForm = Form.find_form("noun")
        Form.adjectiveForm = Form.find_form("adjective")
        Form.prepositionForm = Form.find_form("preposition")
        Form.honorificForm = Form.find_form("honorific")
        Form.honorificAbbreviationForm = Form.find_form("honorific_abbreviation")
    
    adverbForm = -1  # =find_form("adverb")
    nounForm = -1  # =find_form("noun")
    adjectiveForm = -1  # =find_form("adjective")
    prepositionForm = -1  # =find_form("preposition")
    honorificForm = -1  # Form.find_form("honorific")
    honorificAbbreviationForm = -1  # Form.find_form("honorific_abbreviation")
