class VerbMember:
    # String name;
    # Vector <String> members,kinds;
    vbClassTypes = set([ "NO_PHYSICAL_ACTION","CONTROL","CONTACT","NEAR","LOCATIONOBJECT","LOCATIONPREPOBJECT",
        "TRANSFER","NO_PREP_TO","NO_PREP_FROM","MOVE","MOVE_OBJECT","MOVE_IN_PLACE","EXIT",
        "ENTER","CONTIGUOUS","START","STAY","COMMUNICATE","THINK","THINK_OBJECT","SENSE","CREATE",
        "CONSUME","CHANGE_STATE","AGENT_CHANGE_OBJECT_INTERNAL_STATE","META_PROFESSION","META_FUTURE_HAVE",
        "META_FUTURE_CONTACT","META_INFO","META_IF_THEN","META_CONTAINS","META_DESIRE","META_BELIEF","META_ROLE",
        "SPATIAL_ORIENTATION","IGNORE" ])
    
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
                #print("Added " + self.name + " to " + member);
        if childNodes != None:
            for cn in childNodes:
                if cn.find('')==None:
                #if not (childNodes.item(i) instanceof DeferredTextImpl) and !(childNodes.item(i) instanceof DeferredCommentImpl):
                    member = cn.tag
                    if member in self.vbClassTypes:
                        #print("Added kind " + member + " to VerbMember " + self.name);
                        self.kinds.append(member)

    def getName(self): 
        return self.name;
    
    def getNameWithKinds(self):
        names = self.name + " "
        if len(self.kinds) > 0:
            names += "[";
        for kind in self.kinds:
            names += kind + " "
        if len(self.kinds) > 0:
            names += "] "
        return names
