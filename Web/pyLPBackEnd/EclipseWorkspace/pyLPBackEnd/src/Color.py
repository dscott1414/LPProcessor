"""Color.py - Small highlight-colour enum for the web source view.

Overview:
    RED/ORANGE/WHITE/GREEN ids used when colouring tokens (quotes,
    speakers, etc.). No logic.
"""
from enum import Enum

class Color(Enum):
	RED = 1
	ORANGE = 2
	WHITE = 3
	GREEN = 4
