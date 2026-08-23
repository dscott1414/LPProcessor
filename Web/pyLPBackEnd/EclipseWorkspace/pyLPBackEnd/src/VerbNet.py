"""VerbNet.py - Load VerbNet XML classes into vbNetVerbToClassMap.

Overview:
    Walks F:\\lp\\source\\lists\\VerbNet\\*.xml, parses each VNCLASS
    and VNSUBCLASS into VerbMember rows, and indexes verbs by name.
    Lookup helpers return class-name strings or a boolean class test.

Pipeline position:
    Python-side VerbNet, parallel to the C++ VerbNet load during
    initialization. Used when the web backend needs Levin/VN classes.

Key entry points:
    - parse_xml_file(pathName) - one XML class file
    - __init__() - glob all XML under the hardcoded path
    - get_verb_classes / get_class_names / get_class_names_2 / is_verb_class
    - like(str1,str2) - intended prefix compare (currently broken)

Notes / gotchas:
    Hardcoded F:\\lp\\source\\lists\\VerbNet and os.chdir.
    phrasalVerb.length() is a Java-ism (AttributeError on str).
    like() calls str1.len() (should be len) and compares to
    str2[0:min] where `min` is the builtin, not the local m.
"""
from VerbMember import VerbMember
import glob, os
import xml.etree.ElementTree as ET
import jsonpickle
from json import JSONEncoder

class VerbNet:
    
    # Parse one VerbNet XML file: root VNCLASS plus each VNSUBCLASS
    # become a VerbMember appended to self.vms (and the verb map).
    def parse_xml_file(self, pathName):
        # get the factory
        tree = ET.parse(pathName)
        # get the root element
        root = tree.getroot()
        # get a node list of elements named MEMBERS
        if root != None:
            classId = root.get("ID")
            self.vms.append(VerbMember(classId, root.findall("./MEMBERS/MEMBER"), root.findall('*'),self.vbNetVerbToClassMap))
        nl = root.findall(".//VNSUBCLASS")
        if nl != None:
            for m in nl:
                classId = m.get("ID")
                self.vms.append(VerbMember(classId, m.findall("./MEMBERS/MEMBER"),m.findall('*'),self.vbNetVerbToClassMap))

    # chdir to the VerbNet XML directory and parse every *.xml.
    def __init__(self):
        self.vbNetVerbToClassMap = {}
        self.vms = []
        os.chdir("F:\\lp\\source\\lists\\VerbNet")
        for file in glob.glob("*.xml"):
            self.parse_xml_file(file);
            
    # Return the VerbMember list for baseVerb, or for phrasalVerb if
    # that key is present. phrasalVerb.length() will raise on a str.
    def get_verb_classes(self, baseVerb, phrasalVerb):
        vms = self.vbNetVerbToClassMap.get(baseVerb)
        if phrasalVerb.length() > 0:
            vmsParticiple = self.vbNetVerbToClassMap.get(phrasalVerb)
            if vmsParticiple is not None: 
                vms = vmsParticiple
        return vms

    # Intended: prefix-equal of the shorter length. Currently calls
    # .len() (AttributeError) and slices with the builtin `min`.
    def like(self, str1,str2):
        m = min(str1.len(),str2.len());
        return str1[0:m] == str2[0:min]
    
    # Space-joined vm.get_name() for every class of baseVerb, or "".
    def get_class_names(self, baseVerb):
        names = ""
        if len(baseVerb) == 0:
            return names;
        vmsl = self.vbNetVerbToClassMap.get(baseVerb)
        if vmsl is not None:
            for vm in vmsl:
                names += vm.get_name() + " "
        return names
    
    # Like get_class_names but uses get_name_with_kinds().
    def get_class_names_2(self, baseVerb):
        names = "";
        vml = self.vbNetVerbToClassMap.get(baseVerb)
        if vml is not None:
            for vm in vml: 
                names += vm.get_name_with_kinds()
        return names
    
    # True if any VN class of baseVerb/phrasalVerb is like(verbClass).
    def is_verb_class(self, baseVerb, phrasalVerb, verbClass):
        vms = self.get_verb_classes(baseVerb, phrasalVerb)
        if vms is not None:
            for vm in vms:
                if self.like(vm.get_name(),verbClass): 
                    return True
        return False
