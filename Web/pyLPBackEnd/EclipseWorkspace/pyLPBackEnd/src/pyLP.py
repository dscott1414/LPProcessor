'''
Created on May 29, 2022

@author: dscot_000
'''
from flask import Flask, jsonify, request
from flask_cors import CORS
import pymysql
from LPIO import LPIO
from WordClass import WordClass
from Source import Source
from time import perf_counter

app = Flask(__name__)
CORS(app)

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
        print(sql)
        cursor.execute(sql)
        result = cursor.fetchall()
        return_result = []
        for row in result:
            print(row)
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
        print(sql)
        cursor.execute(sql)
        result = cursor.fetchall()
        return_result = []
        for row in result:
            print(row)
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
        print(sql)
        cursor.execute(sql)
        result = cursor.fetchall()
        for row in result:
            sourcePath = row['path']
        connection.close();
    return sourcePath

#sourcePath = "texts\Christie  Agatha  1890-1976\Secret Adversary.txt"


@app.route('/api/loadSource', methods=["GET"])
def loadSource():
    global source
    author = request.args.get('author')
    title = request.args.get('title')
    sourcePath = getPath(author, title)
    source = None
    if sourcePath == "":
        print("Author/title not found")
        return { 'response': [] }
    t_start = perf_counter()
    lpio = LPIO("F:\\lp\\wordFormCache")
    Words = WordClass(lpio);
    lpio.close()
    lpio = LPIO("M:\\caches\\" + sourcePath + ".wordCacheFile")
    Words.readSpecificWordCache(lpio);
    lpio.close()
    lpio = LPIO("M:\\caches\\" + sourcePath + ".SourceCache")
    print("Loading " + sourcePath)
    source = Source(Words, lpio);
    lpio.close()
    preferences = []
    print("Generating HTML Elements for " + sourcePath)
    source.generateSourceElements(Words, preferences)
    t_elapsed = perf_counter() - t_start
    print("loadSource Seconds = " +  "{:.2f}".format(t_elapsed))
    return { 'response': [] }

@app.route('/api/loadElements', methods=["GET"])
def loadElements():
    global source
    width = request.args.get('width')
    height = request.args.get('height')
    fromWhere = request.args.get('fromWhere')
    if source is not None:
        t_start = perf_counter()
        htmlElements = source.printHTML(int(fromWhere), int(width), int(height))
        t_elapsed = perf_counter() - t_start
        print("loadElements Seconds = " +  "{:.2f}".format(t_elapsed))
    else:
        print("Source is not loaded!")
        htmlElements = []
    return { 'response': htmlElements }

@app.route('/api/loadChapters', methods=["GET"])
def loadChapters():
    global source
    if source is not None:
        return { 'response': source.chapters }
    else:
        print("Source is not loaded!")
    return { 'response': {} }


if __name__ == '__main__':  # Script executed directly?
    app.run()  # Launch built-in web server and run this Flask webapp