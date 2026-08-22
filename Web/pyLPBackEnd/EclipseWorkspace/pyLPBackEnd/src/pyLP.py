"""pyLP.py - Flask API that serves deserialized LP parse caches to LPWeb.

Overview:
    Connects to MySQL lp.sources, loads wordFormCache + per-source
    .wordCacheFile / .SourceCache via LPIO, and exposes search, load,
    paging, info-panel, timeline, and matching-object endpoints. Session
    state (preferences, Source, Words) lives in a process-global
    `sessions` dict keyed by a hardcoded uid=1.

Pipeline position:
    Web backend after the C++ parser has written cache files. Not on
    the parse path itself.

Key entry points:
    - connect / login / search_source / search_author / get_path
    - load_source / load_elements / load_chapters / load_agents
    - load_info_panel / load_word_info / load_timeline_segments
    - get_matching_objects / get_surrounding_objects
    - get_mode_switch / set_mode_switch (SSE)

Dependencies:
    MySQL localhost root/byron0; hardcoded F:\\lp\\wordFormCache and
    M:\\caches\\<path>.{wordCacheFile,SourceCache}.

Notes / gotchas:
    Hardcoded DB password and flask secret_key. SQL is concatenated
    LIKE '%'+user+'%' with no escaping. login always sets uid=1
    (comment says change to uuid for multi-user). CORS is wide open.
"""
'''
Created on May 29, 2022

@author: dscot_000
'''
from flask import Flask, request, session
from flask import render_template, Response
from flask_cors import CORS, cross_origin

import pymysql
from LPIO import LPIO
from WordClass import WordClass
from Source import Source
from time import perf_counter
import uuid
import time

app = Flask(__name__)
CORS(app)
app.secret_key = "kjhasd@#$#@"

global sessions
sessions = {}

# Per-browser session: display preferences, mode SSE state, and (after
# load_source) Words / source. Class-level defaults are shared until
# an instance is created and mutated.
class LPSession:
    preferences = { 0: False, 1: False, 2: False, 3: False, 4: False, 5: False }
    modes = {}


@app.route('/')
# GET / — liveness HTML. No session.
def hello():
    return '<h1>Hello, World!</h1>'


# Open a new pymysql connection to lp@localhost as root/byron0.
# Caller must close. DictCursor so rows are dicts.
def connect():
    return pymysql.connect(host='localhost',
                                 port=3306,
                             user='root',
                             password='byron0',
                             database='lp',
                             cursorclass=pymysql.cursors.DictCursor)


@app.route('/api/login', methods=["GET"])
# GET /api/login — stamp flask session uid=1 and allocate an LPSession.
# Not actually multi-user (uid is hardcoded).
def login():
    session['uid'] = 1  # change to uuid.uuid4() when multi user! 
    sessions[session['uid']] = LPSession()
    print("login session established.", session)
    return { 'response': str(session['uid']) }

    
@app.route('/api/searchSource', methods=["GET"])
# GET /api/searchSource?author=&title= — LIKE-search sources, max 10.
# Interpolates author/title into SQL unescaped.
def search_source():
    author = request.args.get('author')
    if author is not None and author == "":
        author = None;
    title = request.args.get('title')
    if title is not None and title == "":
        title = None;
    if author is None and title is None:
        print("No search parameters!")
        return { 'response': [] }
    connection = connect();
    with connection.cursor() as cursor:
        # Read a single record
        sql = "SELECT id, title, author FROM sources WHERE "
        if author is not None:
            sql += "LOWER(author) like LOWER('%" + author + "%') ";
        if author is not None and title is not None:
            sql += "and "
        if title is not None:
            sql += "LOWER(title) like LOWER('%" + title + "%') ";
        sql += "LIMIT 10"
        # print(sql)
        cursor.execute(sql)
        result = cursor.fetchall()
        return_result = []
        for row in result:
            # print(row)
            return_result.append({ 'sourceId': row['id'], 'title': row['title'], 'author': row['author']})
        connection.close();
        return { 'response': return_result }

    
@app.route('/api/searchAuthor', methods=["GET"])
# GET /api/searchAuthor — DISTINCT author LIKE search, same injection risk.
def search_author():
    author = request.args.get('author')
    if author is not None and author == "":
        author = None;
    title = request.args.get('title')
    if title is not None and title == "":
        title = None;
    if author is None and title is None:
        print("No search parameters!")
        return { 'response': [] }
    connection = connect();
    with connection.cursor() as cursor:
        # Read a single record
        sql = "SELECT DISTINCT author FROM sources WHERE "
        if author is not None:
            sql += "LOWER(author) like LOWER('%" + author + "%') ";
        if author is not None and title is not None:
            sql += "and "
        if title is not None:
            sql += "LOWER(title) like LOWER('%" + title + "%') ";
        sql += "LIMIT 10"
        # print(sql)
        cursor.execute(sql)
        result = cursor.fetchall()
        return_result = []
        for row in result:
            # print(row)
            return_result.append({ 'author': row['author']})
        connection.close();
        return { 'response': return_result }

    
# Resolve the first sources.path matching author/title LIKE, or "".
def get_path(author, title):
    if author is not None and author == "":
        author = None;
    if title is not None and title == "":
        title = None;
    if author is None and title is None:
        print("No search parameters!")
        return ""
    connection = connect();
    sourcePath = ""
    with connection.cursor() as cursor:
        # Read a single record
        sql = "SELECT path FROM sources WHERE "
        if author is not None:
            sql += "LOWER(author) like LOWER('%" + author + "%') ";
        if author is not None and title is not None:
            sql += "and "
        if title is not None:
            sql += "LOWER(title) like LOWER('%" + title + "%') ";
        sql += "LIMIT 1"
        # print(sql)
        cursor.execute(sql)
        result = cursor.fetchall()
        for row in result:
            sourcePath = row['path']
        connection.close();
    return sourcePath


@app.route('/api/setPreference', methods=["POST"])
# POST /api/setPreference — store preferences[type]=value on the session.
def set_preference():
    global sessions
    # print(sessions[session['uid']].preferences)
    data = request.get_json()
    # print(data)
    if 'uid' in session and session['uid'] in sessions:
        sessions[session['uid']].preferences[data['type']] = data['value']
        return { 'response': sessions[session['uid']].preferences }
    else:
        return { 'response': {} }

    
@app.route('/api/loadSource', methods=["GET"])
# GET /api/loadSource?author=&title= — load wordFormCache, the source's
# wordCacheFile and SourceCache, then initialize_source_elements +
# generate_per_element_state. Returns empty response; data is in session.
def load_source():
    global sessions
    t_gstart = perf_counter()
    author = request.args.get('author')
    title = request.args.get('title')
    sourcePath = get_path(author, title)
    if sourcePath == "":
        print("Author/title not found")
        return { 'response': [] }

    if 'uid' not in session or session['uid'] not in sessions:
        print("Session not logged in")
        return { 'response': [] }
        
    t_start = perf_counter()
    lpio = LPIO("F:\\lp\\wordFormCache")
    sessions[session['uid']].Words = WordClass(lpio);
    lpio.close()
    t_elapsed = perf_counter() - t_start
    print("loadSource WordClass Seconds = " + "{:.2f}".format(t_elapsed))

    t_start = perf_counter()
    lpio = LPIO("M:\\caches\\" + sourcePath + ".wordCacheFile")
    sessions[session['uid']].Words.read_specific_word_cache(lpio);
    lpio.close()
    t_elapsed = perf_counter() - t_start
    print("loadSource read_specific_word_cache Seconds = " + "{:.2f}".format(t_elapsed))

    t_start = perf_counter()
    lpio = LPIO("M:\\caches\\" + sourcePath + ".SourceCache")
    print("Loading " + sourcePath)
    sessions[session['uid']].source = Source(sessions[session['uid']].Words, lpio);
    lpio.close()
    t_elapsed = perf_counter() - t_start
    print("loadSource SourceCache Seconds = " + "{:.2f}".format(t_elapsed))

    t_start = perf_counter()
    sessions[session['uid']].source.initialize_source_elements()
    t_elapsed = perf_counter() - t_start
    print("loadSource initialize_source_elements Seconds = " + "{:.2f}".format(t_elapsed))

    t_start = perf_counter()
    sessions[session['uid']].source.states = sessions[session['uid']].source.generate_per_element_state()
    t_elapsed = perf_counter() - t_start
    print("loadSource generate_per_element_state Seconds = " + "{:.2f}".format(t_elapsed))
    t_elapsed = perf_counter() - t_gstart
    print("loadSource TOTAL Seconds = " + "{:.2f}".format(t_elapsed))
    
    return { 'response': [] }


@app.route('/api/loadElements', methods=["GET"])
# GET /api/loadElements?width=&height=&fromWhere=src.el — page of HTML
# elements from print_html, optionally finishing the current source index
# first. fromWhere is "sourceIndex.elementIndex".
def load_elements():
    global sessions
    width = request.args.get('width')
    height = request.args.get('height')
    fromWhere = request.args.get('fromWhere') # id is source index.element index (100.2)
    fromWhereSplit = fromWhere.split('.')
    sourceIndex = int(fromWhereSplit[0])
    elementIndex = int(fromWhereSplit[1])
    htmlElements = []
    currentWidth = 0
    currentHeight = 0
    if elementIndex != 0:
        for ei in range(elementIndex,len(sessions[session['uid']].source.batchDoc[sourceIndex])):
            htmlElement = sessions[session['uid']].source.batchDoc[sourceIndex][ei]
            htmlElements.append(htmlElement)
            currentWidth, currentHeight = sessions[session['uid']].source.advance_screen_position(htmlElement, int(width), currentWidth, currentHeight)
        sourceIndex += 1
    if 'uid' in session and session['uid'] in sessions:
        t_start = perf_counter()
        htmlElements = sessions[session['uid']].source.print_html(sessions[session['uid']].preferences, sourceIndex, currentWidth, currentHeight, int(width), int(height))
        t_elapsed = perf_counter() - t_start
        print("loadElements Seconds = " + "{:.2f}".format(t_elapsed))
    else:
        print("Source is not loaded!")
        htmlElements = []
    return { 'response': htmlElements }


@app.route('/api/loadChapters', methods=["GET"])
# GET /api/loadChapters — source.chapters or {}.
def load_chapters():
    global sessions
    if 'uid' in session and session['uid'] in sessions:
        return { 'response': sessions[session['uid']].source.chapters }
    else:
        print("Source is not loaded!")
    return { 'response': {} }


@app.route('/api/loadAgents', methods=["GET"])
# GET /api/loadAgents — master_speaker_html for each masterSpeakerList slot.
def load_agents():
    global sessions
    if 'uid' in session and session['uid'] in sessions:
        htmlElements = []
        for ms in range(len(sessions[session['uid']].source.masterSpeakerList)):
            html = sessions[session['uid']].source.master_speaker_html(ms)
            # print(ms,html)
            htmlElements.append(html)
        return { 'response': htmlElements }
    else:
        print("Source is not loaded!")
    return { 'response': [] }


@app.route('/api/loadInfoPanel', methods=["GET"])
# GET /api/loadInfoPanel?where= — word/role/relations/tooltip for a token.
def load_info_panel():
    global sessions
    where = request.args.get('where')
    if 'uid' in session and session['uid'] in sessions:
        wordInfo, roleInfo, relationsInfo, toolTip = sessions[session['uid']].source.get_info_panel(where)
        return { 'response': { "wordInfo":wordInfo, "roleInfo":roleInfo, "relationsInfo": relationsInfo, "toolTip":toolTip } }
    else:
        print("Source is not loaded!")
    return { 'response': {} }

@app.route('/api/wordInfo', methods=["GET"])
# GET /api/wordInfo — paged get_word_info(beginId, endId, page, size).
def load_word_info():
    global sessions
    beginId = request.args.get('beginId')
    endId = request.args.get('endId')
    pageNumber = request.args.get('pageNumber')
    pageSize = request.args.get('pageSize')
    if 'uid' in session and session['uid'] in sessions:
        wordInfo = sessions[session['uid']].source.get_word_info(beginId, endId, pageNumber, pageSize)
        return { 'response': wordInfo }
    else:
        print("Source is not loaded!")
    return { 'response': {} }

@app.route('/api/timelineSegments', methods=["GET"])
# GET /api/timelineSegments — paged timeline segments.
def load_timeline_segments():
    global sessions
    pageNumber = request.args.get('pageNumber')
    pageSize = request.args.get('pageSize')
    if 'uid' in session and session['uid'] in sessions:
        segments = sessions[session['uid']].source.get_timeline_segments(pageNumber, pageSize)
        return { 'response': segments }
    else:
        print("Source is not loaded!")
    return { 'response': {} }

@app.route('/api/HTMLElementIdToSource', methods=["GET"])
# GET /api/HTMLElementIdToSource?elementId= — map HTML id to source index.
def html_element_id_to_source():
    global sessions
    elementId = request.args.get('elementId')
    if 'uid' in session and session['uid'] in sessions:
        elementId = sessions[session['uid']].source.HTMLElementIdToSource[int(elementId)]
        return { 'response': elementId }
    else:
        print("Source is not loaded!")
    return { 'response': -1 }

# Offline load-loop for profiling: 100x Source("adv") + initialize.
# generate_per_element_state is called with extra args the method
# does not take (will TypeError if run).
def test():
    sourcePath = get_path("", "adv")
    if sourcePath == "":
        print("Author/title not found")
        return { 'response': [] }
    lpio = LPIO("F:\\lp\\wordFormCache")
    Words = WordClass(lpio)
    lpio.close()
    lpio = LPIO("M:\\caches\\" + sourcePath + ".wordCacheFile")
    Words.read_specific_word_cache(lpio)
    lpio.close()
    for _ in range(100):
        lpio = LPIO("M:\\caches\\" + sourcePath + ".SourceCache")
        source = Source(Words, lpio)
        lpio.close()
        preferences = { 0: False, 1: False, 2: False, 3: False, 4: False, 5: False }
        source.initialize_source_elements()
        source.states = source.generate_per_element_state(Words, preferences)

@app.route('/api/searchStringList', methods=["GET"])
# GET /api/searchStringList — get_instances_of_word(sourceSearchString).
def search_string_list():
    global sessions
    sourceSearchString = request.args.get('sourceSearchString')
    if 'uid' in session and session['uid'] in sessions:
        searchResults = sessions[session['uid']].source.get_instances_of_word(sourceSearchString)
        print(searchResults)
        return { 'response': searchResults }
    else:
        print("Source is not loaded!")
    return { 'response': [] }
    
@app.route('/api/findParagraphStart', methods=["GET"])
# GET /api/findParagraphStart — source index of the paragraph start.
def find_paragraph_start():
    global sessions
    somewhereInParagraph = request.args.get('somewhereInParagraph')
    if 'uid' in session and session['uid'] in sessions:
        paragraphStartId = sessions[session['uid']].source.find_paragraph_start(somewhereInParagraph)
        print(paragraphStartId)
        return { 'response': paragraphStartId }
    else:
        print("Source is not loaded!")
    return { 'response': "" }
    
@app.route('/api/getElementType', methods=["GET"])
# GET /api/getElementType — SourceMapType name for an HTML element id.
def get_element_type():
    global sessions
    elementId = request.args.get('elementId')
    if 'uid' in session and session['uid'] in sessions:
        elementType = sessions[session['uid']].source.get_element_type(elementId)
        print(elementType)
        return { 'response': elementType }
    else:
        print("Source is not loaded!")
    return { 'response': "" }
    
@app.route('/stream-get-mode-switch', methods=["GET"])
@cross_origin(origins=['http://127.0.0.1:4200'])
# SSE: poll sessions[suid].modes[mode] every second and yield when it
# changes. CORS locked to http://127.0.0.1:4200.
def get_mode_switch():
    global sessions
    mode = request.args.get('mode')
    suid = int(request.args.get('suid'))
    # Infinite generator; sleeps 1s between polls.
    def event_stream():
        last_mode_info = None
        while True:
            time.sleep(1)
            if suid in sessions and mode in sessions[suid].modes:
                current_mode_info = sessions[suid].modes[mode]
                if current_mode_info != last_mode_info:
                    last_mode_info = current_mode_info
                    yield f"data:{mode}\n\n"
    return Response(event_stream(), mimetype='text/event-stream')
    
@app.route('/set-mode-switch', methods=["GET"])
# GET /set-mode-switch — write modes[mode]=mode_info for suid.
def set_mode_switch():
    global sessions
    mode = request.args.get('mode')
    current_mode_info = request.args.get('mode_info')
    suid = request.args.get('suid')
    if suid in sessions:
        sessions[suid].modes[mode] = current_mode_info
        return { 'response': current_mode_info }
    else:
        print("Source is not loaded!")
    return { 'response': "" }
    
@app.route('/api/getMatchingObjects', methods=["GET"])
# GET /api/getMatchingObjects — objects matching an HTML element's type.
def get_matching_objects():
    global sessions
    elementId = request.args.get('elementId')
    matchingType = request.args.get('matchingType')
    if 'uid' in session and session['uid'] in sessions:
        matchingObjects = sessions[session['uid']].source.get_matching_objects(elementId, matchingType)
        print(matchingObjects)
        return { 'response': matchingObjects }
    else:
        print("Source is not loaded!")
    return { 'response': "" }
    
@app.route('/api/getSurroundingObjects', methods=["GET"])
# GET /api/getSurroundingObjects — nearby objects of matchingType.
def get_surrounding_objects():
    global sessions
    elementId = request.args.get('elementId')
    matchingType = request.args.get('matchingType')
    if 'uid' in session and session['uid'] in sessions:
        surroundingObjects = sessions[session['uid']].source.get_surrounding_objects(elementId, matchingType)
        print(surroundingObjects)
        return { 'response': surroundingObjects }
    else:
        print("Source is not loaded!")
    return { 'response': "" }
    
if __name__ == '__main__':  # Script executed directly?
    app.run()  # Launch built-in web server and run this Flask webapp

# cProfile.run("test()", filename = "out.prof")
# p = pstats.Stats("out.prof")
# p.sort_stats(pstats.SortKey.TIME).print_stats(10);
