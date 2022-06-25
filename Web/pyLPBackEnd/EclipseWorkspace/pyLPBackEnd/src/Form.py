class Form:

    forms = {}
    def __init__(self,rs):
        self.name = rs.readString()
        self.shortName=rs.readString()
        self.inflectionsClass=rs.readString()
        self.hasInflections = rs.readShort()!=0
        self.properNounSubClass = rs.readShort()!=0
        self.isTopLevel = rs.readShort()!=0
        self.isIgnore = rs.readShort()!=0
        self.verbForm = rs.readShort()!=0
        self.blockProperNounRecognition = rs.readShort()!=0
        self.formCheck = rs.readShort()!=0

    @staticmethod
    def findForm(name):
        for f in range(len(Form.forms)):
            if (Form.forms[f].name==name):
                return f
        return -1
    
    @staticmethod
    def initializeForms():
        Form.adverbForm=Form.findForm("adverb")
        Form.nounForm=Form.findForm("noun")
        Form.adjectiveForm=Form.findForm("adjective")
        Form.prepositionForm=Form.findForm("preposition")
        Form.honorificForm = Form.findForm("honorific")
        Form.honorificAbbreviationForm = Form.findForm("honorific_abbreviation")
    
    adverbForm=-1 #=findForm("adverb")
    nounForm=-1 #=findForm("noun")
    adjectiveForm=-1 #=findForm("adjective")
    prepositionForm=-1 #=findForm("preposition")
    honorificForm = -1 #Form.findForm("honorific")
    honorificAbbreviationForm = -1 #Form.findForm("honorific_abbreviation")