from CObject import CObject
import pymysql
from LPIO import LPIO
from WordClass import WordClass
from Source import Source
import json

sourcePath = "texts\Christie  Agatha  1890-1976\Secret Adversary.txt"
lpio = LPIO("F:\\lp\\wordFormCache")
Words = WordClass(lpio);
lpio.close()
lpio = LPIO("M:\\caches\\" + sourcePath + ".wordCacheFile")
Words.readSpecificWordCache(lpio);
lpio.close()
lpio = LPIO("M:\\caches\\" + sourcePath + ".SourceCache")
source = Source(Words, lpio);
lpio.close()
preferences = []
elements = source.generateSourceElements(Words, preferences)
print(source.printHTML(0, 200, 200))
#print(source.generateCSSClasses())
#print(json.dumps(elements, indent=2))
