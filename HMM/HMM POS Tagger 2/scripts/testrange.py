noun_suffix = ["action", "age", "ance", "cy", "dom", "ee", "ence", "er", "hood", "ion", "ism", "ist", "ity", "ling", "ment", "ness", "or", "ry", "scape", "ship", "ty"]
tok="vantage"
if any(tok.endswith(suffix) for suffix in noun_suffix):
	print("YES")
	
