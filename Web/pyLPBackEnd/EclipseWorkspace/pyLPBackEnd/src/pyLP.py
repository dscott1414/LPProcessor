'''
Created on May 29, 2022

@author: dscot_000
'''
from flask import Flask, request, session
from flask_cors import CORS
import pymysql
from LPIO import LPIO
from WordClass import WordClass
from Source import Source
from time import perf_counter
import uuid
import json

app = Flask(__name__)
CORS(app)
app.secret_key = "kjhasd@#$#@"

global sessions
sessions = {}


class LPSession:
    pass


try:
    profile  # throws an exception when profile isn't defined
except NameError:
    profile = lambda x: x  # if it's not defined simply ignore the decorator.


@app.route('/')
def hello():
    return '<h1>Hello, World!</h1>'


def connect():
    return pymysql.connect(host='localhost',
                                 port=3306,
                             user='root',
                             password='byron0',
                             database='lp',
                             cursorclass=pymysql.cursors.DictCursor)


@app.route('/api/login', methods=["GET"])
def login():
    session['uid'] = 1  # change to uuid.uuid4() when multi user! 
    sessions[session['uid']] = LPSession()
    sessions[session['uid']].preferences = { 0: False, 1: False, 2: False, 3: False, 4: False, 5: False }
    return { 'response': str(session['uid']) }

    
@app.route('/api/searchSource', methods=["GET"])
def searchSource():
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
def searchAuthor():
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

    
def getPath(author, title):
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
def setPreference():
    global sessions
    # print(sessions[session['uid']].preferences)
    data = request.get_json()
    # print(data)
    sessions[session['uid']].preferences[data['type']] = data['value']
    return { 'response': sessions[session['uid']].preferences }

    
@profile
@app.route('/api/loadSource', methods=["GET"])
def loadSource():
    global sessions
    author = request.args.get('author')
    title = request.args.get('title')
    sourcePath = getPath(author, title)
    if sourcePath == "":
        print("Author/title not found")
        return { 'response': [] }
    t_start = perf_counter()
    lpio = LPIO("F:\\lp\\wordFormCache")
    sessions[session['uid']].Words = WordClass(lpio);
    lpio.close()
    lpio = LPIO("M:\\caches\\" + sourcePath + ".wordCacheFile")
    sessions[session['uid']].Words.readSpecificWordCache(lpio);
    lpio.close()
    lpio = LPIO("M:\\caches\\" + sourcePath + ".SourceCache")
    print("Loading " + sourcePath)
    sessions[session['uid']].source = Source(sessions[session['uid']].Words, lpio);
    lpio.close()
    t_elapsed = perf_counter() - t_start
    print("loadSource Seconds = " + "{:.2f}".format(t_elapsed))
    return { 'response': [] }


@profile
@app.route('/api/generateSourceElements', methods=["GET"])
def generateSourceElements():
    global sessions
    t_start = perf_counter()
    print("Generating HTML Elements with preferences:")
    # print(sessions[session['uid']].preferences)
    sessions[session['uid']].source.generateSourceElements(sessions[session['uid']].Words, sessions[session['uid']].preferences)
    for c in sessions[session['uid']].source.chapters:
        c['position'] = sessions[session['uid']].source.sourceToHTMLElementPosition[c['where']][0]
    t_elapsed = perf_counter() - t_start
    print("generateSourceElements Seconds = " + "{:.2f}".format(t_elapsed))
    return { 'response': [] }


@profile
@app.route('/api/loadElements', methods=["GET"])
def loadElements():
    global sessions
    width = request.args.get('width')
    height = request.args.get('height')
    fromWhere = request.args.get('fromWhere')
    if sessions[session['uid']] is not None:
        t_start = perf_counter()
        htmlElements = sessions[session['uid']].source.printHTML(int(fromWhere), int(width), int(height))
        t_elapsed = perf_counter() - t_start
        print("loadElements Seconds = " + "{:.2f}".format(t_elapsed))
    else:
        print("Source is not loaded!")
        htmlElements = []
    return { 'response': htmlElements }


@app.route('/api/loadChapters', methods=["GET"])
def loadChapters():
    global sessions
    if sessions[session['uid']] is not None:
        return { 'response': sessions[session['uid']].source.chapters }
    else:
        print("Source is not loaded!")
    return { 'response': {} }


@app.route('/api/loadAgents', methods=["GET"])
def loadAgents():
    global sessions
    if sessions[session['uid']] is not None:
        htmlElements = []
        for ms in range(len(sessions[session['uid']].source.masterSpeakerList)):
            html = sessions[session['uid']].source.masterSpeakerHtml(ms)
            # print(ms,html)
            htmlElements.append(html)
        return { 'response': htmlElements }
    else:
        print("Source is not loaded!")
    return { 'response': [] }


@app.route('/api/loadInfoPanel', methods=["GET"])
def loadInfoPanel():
    global sessions
    where = request.args.get('where')
    wordInfo, roleInfo, relationsInfo, toolTip = sessions[session['uid']].source.getInfoPanel(int(where))
    if sessions[session['uid']] is not None:
        return { 'response': { "wordInfo":wordInfo, "roleInfo":roleInfo, "relationsInfo": relationsInfo, "toolTip":toolTip } }
    else:
        print("Source is not loaded!")
    return { 'response': {} }

@app.route('/api/wordInfo', methods=["GET"])
def loadWordInfo():
    global sessions
    beginId = request.args.get('beginId')
    endId = request.args.get('endId')
    pageNumber = request.args.get('pageNumber')
    pageSize = request.args.get('pageSize')
    wordInfo = sessions[session['uid']].source.getWordInfo(beginId, endId, pageNumber, pageSize)
    if sessions[session['uid']] is not None:
        return { 'response': wordInfo }
    else:
        print("Source is not loaded!")
    return { 'response': {} }

@app.route('/api/timelineSegments', methods=["GET"])
def loadTimelineSegments():
    global sessions
    pageNumber = request.args.get('pageNumber')
    pageSize = request.args.get('pageSize')
    segments = sessions[session['uid']].source.getTimelineSegments(pageNumber, pageSize)
    if sessions[session['uid']] is not None:
        return { 'response': segments }
    else:
        print("Source is not loaded!")
    return { 'response': {} }

@app.route('/api/HTMLElementIdToSource', methods=["GET"])
def HTMLElementIdToSource():
    global sessions
    elementId = request.args.get('elementId')
    if sessions[session['uid']] is not None:
        elementId = sessions[session['uid']].source.HTMLElementIdToSource[int(elementId)]
        return { 'response': elementId }
    else:
        print("Source is not loaded!")
    return { 'response': -1 }

# if __name__ == '__main__':  # Script executed directly?
#    app.run()  # Launch built-in web server and run this Flask webapp

def test():
    sourcePath = getPath("", "adv")
    if sourcePath == "":
        print("Author/title not found")
        return { 'response': [] }
    lpio = LPIO("F:\\lp\\wordFormCache")
    session['Words'] = WordClass(lpio)
    lpio.close()
    lpio = LPIO("M:\\caches\\" + sourcePath + ".wordCacheFile")
    session['Words'].readSpecificWordCache(lpio)
    lpio.close()
    lpio = LPIO("M:\\caches\\" + sourcePath + ".SourceCache")
    session['source'] = Source(session['Words'], lpio)
    lpio.close()
    session['preferences'] = { 0: False, 1: False, 2: False, 3: False, 4: False, 5: False }
    session['source'].generateSourceElements(session['Words'], session['preferences'])
    for c in session['source'].chapters:
        c['position'] = session['source'].sourceToHTMLElementPosition[c['where']][0]

session = {}
test()
