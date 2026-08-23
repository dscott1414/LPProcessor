"""VerbMember.py - One VerbNet class: name, member verbs, LP kind tags.

Overview:
    Built by VerbNet.parse_xml_file. Collects MEMBER@name into members
    and indexes each in vbNetVerbToClassMap. Child XML tags that match
    vbClassTypes (MOVE, THINK, COMMUNICATE, …) become kinds — the LP
    semantic labels overlaid on VerbNet classes.

Pipeline position:
    Initialization of the Python VerbNet; queried via VerbNet.get_*.
"""
class VerbMember:
    # String name;
    # Vector <String> members,kinds;
    vbClassTypes = set([ "NO_PHYSICAL_ACTION","CONTROL","CONTACT","NEAR","LOCATIONOBJECT","LOCATIONPREPOBJECT",
        "TRANSFER","NO_PREP_TO","NO_PREP_FROM","MOVE","MOVE_OBJECT","MOVE_IN_PLACE","EXIT",
        "ENTER","CONTIGUOUS","START","STAY","COMMUNICATE","THINK","THINK_OBJECT","SENSE","CREATE",
        "CONSUME","CHANGE_STATE","AGENT_CHANGE_OBJECT_INTERNAL_STATE","META_PROFESSION","META_FUTURE_HAVE",
        "META_FUTURE_CONTACT","META_INFO","META_IF_THEN","META_CONTAINS","META_DESIRE","META_BELIEF","META_ROLE",
        "SPATIAL_ORIENTATION","IGNORE" ])
    
    # n=class id, nl=MEMBER elements, childNodes=all children (kinds).
    # Mutates vbNetVerbToClassMap: each member name appends `self`.
    def __init__(self, n, nl, childNodes, vbNetVerbToClassMap):
        self.name = n
        self.members = []
        self.kinds = []
        if nl != None:
            for m in nl:
                member = m.get("name")
                self.members.append(member)
                if member not in vbNetVerbToClassMap:
                    vbNetVerbToClassMap[member] = []
                vbNetVerbToClassMap[member].append(self)
                # print("Added " + self.name + " to " + member);
        if childNodes != None:
            for cn in childNodes:
                member = cn.tag
                if member in self.vbClassTypes:
                    # print("Added kind " + member + " to VerbMember " + self.name);
                    self.kinds.append(member)
                # else:
                    # print("Did not add kind " + member + " to VerbMember " + self.name);

    # VerbNet class id (e.g. "hit-18.1").
    def get_name(self): 
        return self.name;
    
    # "name [KIND KIND ] " when kinds is non-empty.
    def get_name_with_kinds(self):
        names = self.name + " "
        if len(self.kinds) > 0:
            names += "[";
        for kind in self.kinds:
            names += kind + " "
        if len(self.kinds) > 0:
            names += "] "
        return names
