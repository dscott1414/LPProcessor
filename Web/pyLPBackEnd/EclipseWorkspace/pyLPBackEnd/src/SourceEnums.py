class SourceEnums:
    PRONOUN_OBJECT_CLASS = 1
    REFLEXIVE_PRONOUN_OBJECT_CLASS = 2
    RECIPROCAL_PRONOUN_OBJECT_CLASS = 3
    NAME_OBJECT_CLASS = 4
    GENDERED_GENERAL_OBJECT_CLASS = 5
    BODY_OBJECT_CLASS = 6
    GENDERED_OCC_ROLE_ACTIVITY_OBJECT_CLASS = 7  # occupation=plumberrole=leaderactivity=runner
    GENDERED_DEMONYM_OBJECT_CLASS = 8
    NON_GENDERED_GENERAL_OBJECT_CLASS = 9
    NON_GENDERED_BUSINESS_OBJECT_CLASS = 10
    PLEONASTIC_OBJECT_CLASS = 11
    # DEICTIC_OBJECT_CLASS=7, not necessary
    VERB_OBJECT_CLASS = 12  # the news I brought /
    NON_GENDERED_NAME_OBJECT_CLASS = 13
    META_GROUP_OBJECT_CLASS = 14
    GENDERED_RELATIVE_OBJECT_CLASS = 15

    UNDEFINED_FORM_NUM = 0
    SECTION_FORM_NUM = 1
    COMBINATION_FORM_NUM = 2
    PROPER_NOUN_FORM_NUM = 3
    NUMBER_FORM_NUM = 4
    HONORIFIC_FORM_NUM = 5

    T_ASSUME_SEQUENTIAL = 0
    MTM = 1
    # on the following lines, flags cannot be combined, only one may be active at
    # a time
    T_BEFORE = MTM
    T_AFTER = MTM * 2
    T_PRESENT = MTM * 3
    T_THROUGHOUT = MTM * 4
    T_RECURRING = MTM * 5
    T_AT = MTM * 6
    T_MIDWAY = MTM * 7
    T_IN = MTM * 8
    T_ON = MTM * 9
    T_INTERVAL = MTM * 10
    T_START = MTM * 11
    T_STOP = MTM * 12
    T_RESUME = MTM * 13
    T_FINISH = MTM * 14
    T_RANGE = MTM * 15
    T_UNIT = MTM * 16
    # on the following lines, flags can be combined
    T_TIME = MTM * 32
    T_DATE = MTM * 64
    T_TIMEDATE = T_TIME + T_DATE
    T_VAGUE = MTM * 128
    T_LENGTH = MTM * 256
    T_CARDTIME = MTM * 512

    UNKNOWN_OBJECT = -1
    OBJECT_UNKNOWN_MALE = -2
    OBJECT_UNKNOWN_FEMALE = -3
    OBJECT_UNKNOWN_MALE_OR_FEMALE = -4
    OBJECT_UNKNOWN_NEUTER = -5
    OBJECT_UNKNOWN_PLURAL = -6
    OBJECT_UNKNOWN_ALL = -7

    CANADIAN_PROVINCE_CITY = 0
    COUNTRY = 1
    ISLAND = 2
    MOUNTAIN_RANGE_PEAK_LANDFORM = 3
    OCEAN_SEA = 4
    PARK_MONUMENT = 5
    REGION = 6
    RIVER_LAKE_WATERWAY = 7
    US_CITY_TOWN_VILLAGE = 8
    US_STATE_TERRITORY_REGION = 9
    WORLD_CITY_TOWN_VILLAGE = 10
    GEOGRAPHICAL_NATURAL_FEATURE = 11  # lake mountain
    GEOGRAPHICAL_URBAN_FEATURE = 12  # city, town,
    GEOGRAPHICAL_URBAN_SUBFEATURE = 13  # rooms within
    GEOGRAPHICAL_URBAN_SUBSUBFEATURE = 14  # commonly
    TRAVEL = 15  # trip, journey, road, street, trail
    MOVING = 16  # train, plane, automobile
    MOVING_NATURAL = 17  # elephant, horse, _NAME (human)
    RELATIVE_DIRECTION = 18  # front, rear, side, edge,
    ABSOLUTE_DIRECTION = 19  # North, South, East, West
    BY_ACTIVITY = 20  # lunch, dinner, breakfast, party,
    UNKNOWN_PLACE_SUBTYPE = 21

    # "time","space","all other","story","quote","non-time transition"
    INCLUDE_TIME = 0
    INCLUDE_SPACE = 1
    INCLUDE_ABSTRACT = 2
    INCLUDE_STORY = 3
    INCLUDE_QUOTES = 4
    INCLUDE_TIME_TRANSITION = 5

    stEXIT = 1
    stENTER = 2
    stSTAY = 3
    stESTABLISH = 4
    stMOVE = 5
    stMOVE_OBJECT = 6
    stMOVE_IN_PLACE = 7
    stMETAWQ = 8
    stCONTACT = 9
    stNEAR = 10
    stTRANSFER = 11
    stLOCATION = 12
    stPREPTIME = 13
    stPREPDATE = 14
    stSUBJDAYOFMONTHTIME = 15
    stABSTIME = 16
    stABSDATE = 17
    stADVERBTIME = 18
    stTHINK = 19
    stCOMMUNICATE = 20
    stSTART = 21
    stCHANGE_STATE = 22
    stBE = 23
    stHAVE = 24
    stMETAINFO = 25
    stMETAPROFESSION = 26
    stMETABELIEF = 27
    stTHINKOBJECT = 29
    stCHANGESTATE = 30

    VT_TENSE_MASK = 7
    VT_PRESENT = 1
    VT_PRESENT_PERFECT = 2
    VT_PAST = 3
    VT_PAST_PERFECT = 4
    VT_FUTURE = 5
    VT_FUTURE_PERFECT = 6
    VT_POSSIBLE = 8
    VT_PASSIVE = 16
    VT_EXTENDED = 32
    VT_VERB_CLAUSE = 64
    VT_NEGATION = 128
    VT_IMPERATIVE = 256

