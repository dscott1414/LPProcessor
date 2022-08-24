from VerbMember import VerbMember
import glob, os
import xml.etree.ElementTree as ET
import jsonpickle
from json import JSONEncoder

class VerbNet:
    
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

    def __init__(self):
        self.vbNetVerbToClassMap = {}
        self.vms = []
        os.chdir("F:\\lp\\source\\lists\\VerbNet")
        for file in glob.glob("*.xml"):
            self.parse_xml_file(file);
            
    def get_verb_classes(self, baseVerb, phrasalVerb):
        vms = self.vbNetVerbToClassMap.get(baseVerb)
        if phrasalVerb.length() > 0:
            vmsParticiple = self.vbNetVerbToClassMap.get(phrasalVerb)
            if vmsParticiple is not None: 
                vms = vmsParticiple
        return vms

    def like(self, str1,str2):
        m = min(str1.len(),str2.len());
        return str1[0:m] == str2[0:min]
    
    def get_class_names(self, baseVerb):
        names = ""
        if len(baseVerb) == 0:
            return names;
        vmsl = self.vbNetVerbToClassMap.get(baseVerb)
        if vmsl is not None:
            for vm in vmsl:
                names += vm.get_name() + " "
        return names
    
    def get_class_names_2(self, baseVerb):
        names = "";
        vml = self.vbNetVerbToClassMap.get(baseVerb)
        if vml is not None:
            for vm in vml: 
                names += vm.get_name_with_kinds()
        return names
    
    def is_verb_class(self, baseVerb, phrasalVerb, verbClass):
        vms = self.get_verb_classes(baseVerb, phrasalVerb)
        if vms is not None:
            for vm in vms:
                if self.like(vm.get_name(),verbClass): 
                    return True
        return False
