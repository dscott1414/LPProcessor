"""test.py - Manual entry that constructs a dummy pyLP.session and runs pyLP.test().

Overview:
    Scratch harness for the Flask/web backend. Imports CObject, pymysql,
    LPIO, WordClass, Source, json but only uses pyLP. Not a unit test.
"""
from CObject import CObject
import pymysql
from LPIO import LPIO
from WordClass import WordClass
from Source import Source
import json
import pyLP

pyLP.session = {}
pyLP.test()
