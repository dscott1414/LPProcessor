class Form:

    forms = {}

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
    def find_form(name):
        for f in range(len(Form.forms)):
            if (Form.forms[f].name == name):
                return f
        return -1
    
    @staticmethod
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
