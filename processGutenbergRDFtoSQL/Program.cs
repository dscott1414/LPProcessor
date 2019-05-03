using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Xml.Linq;
using System.Xml;
using MySql.Data;
using MySql.Data.MySqlClient;
using System.IO;

namespace processGutenbergRDFtoSQL
{
    public static class processGutenbergRDFtoSQL
    {
        static bool test = false;
        static XNamespace nsGutenbergTerms = "http://www.gutenberg.org/rdfterms/";
        static XNamespace nsRdf = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
        static XNamespace rdf = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
        static XNamespace rdfs = "http://www.w3.org/2000/01/rdf-schema#";
        static XNamespace dc = "http://purl.org/dc/elements/1.1/";
        static XNamespace dcterms = "http://purl.org/dc/terms/";
        static XNamespace dcmitype = "http://purl.org/dc/dcmitype/";
        static XNamespace cc = "http://web.resource.org/cc/";
        static XNamespace pgterms = "http://www.gutenberg.org/2009/pgterms/";
        static XNamespace dcam = "http://purl.org/dc/dcam/";
        static XNamespace marcrel = "http://id.loc.gov/vocabulary/relators/";
        /// <summary>
        /// Returns a sequence of <see cref="XElement">XElements</see> corresponding to the currently
        /// positioned element and all following sibling elements which match the specified name.
        /// </summary>
        /// <param name="reader">The xml reader positioned at the desired hierarchy level.</param>
        /// <param name="elementName">An <see cref="XName"/> representing the name of the desired element.</param>
        /// <returns>A sequence of <see cref="XElement">XElements</see>.</returns>
        /// <remarks>At the end of the sequence, the reader will be positioned on the end tag of the parent element.</remarks>
        public static IEnumerable<XElement> ReadElements(this XmlReader reader, XName elementName)
        {
            if (reader.Name == elementName.LocalName && reader.NamespaceURI == elementName.NamespaceName)
                yield return (XElement)XElement.ReadFrom(reader);

            while (reader.ReadToNextSibling(elementName.LocalName, elementName.NamespaceName))
                yield return (XElement)XElement.ReadFrom(reader);
        }

        public static HashSet<string> getExistingSources()
        {
            MySqlConnection queryConnection = new MySqlConnection(string.Format("Server=localhost; database={0}; UID={1}; password={2}", "lp", "root", "byron0"));
            queryConnection.Open();
            var querycmd = new MySqlCommand("select etext from sources", queryConnection);
            var etextReader = querycmd.ExecuteReader();
            HashSet<string> etexts = new HashSet<string>();
            if (etextReader.HasRows)
                while (etextReader.Read())
                    if (!etextReader.IsDBNull(0))
                        etexts.Add(etextReader.GetString(0));
            etextReader.Close();
            queryConnection.Close();
            return etexts;
        }

        // not needed anymore, kept in case we need to do something like this again
        public static void checkExistingSources()
        {
            int incorrectPaths = 0,correctPaths=0,correctedPaths=0;
            List<string> etexts=new List<string>(), authors= new List<string>(), titles= new List<string>(), paths= new List<string>();
            MySqlConnection queryConnection = new MySqlConnection(string.Format("Server=localhost; database={0}; UID={1}; password={2}", "lp", "root", "byron0"));
            queryConnection.Open();
            var querycmd = new MySqlCommand("select etext,author,title,path from sources where sourceType=2 and start='**FIND**'", queryConnection);
            var etextReader = querycmd.ExecuteReader();
            if (etextReader.HasRows)
                while (etextReader.Read())
                    if (!etextReader.IsDBNull(0))
                    {
                        if (!File.Exists("J:\\caches\\" + etextReader.GetString(3)))
                        {
                            etexts.Add(etextReader.GetString(0));
                            authors.Add(etextReader.GetString(1));
                            titles.Add(etextReader.GetString(2));
                            paths.Add(etextReader.GetString(3));
                            Console.WriteLine("etext={0} author={1},title={2},path={3}",
                                etextReader.GetString(0), etextReader.GetString(1), etextReader.GetString(2), etextReader.GetString(3));
                            incorrectPaths++;
                        }
                        else
                            correctPaths++;
                    }
            etextReader.Close();
            queryConnection.Close();
            MySqlConnection updateConnection = new MySqlConnection(string.Format("Server=localhost; database={0}; UID={1}; password={2}", "lp", "root", "byron0"));
            updateConnection.Open();
            for (int I=0; I<etexts.Count; I++)
            {
                string etext = etexts[I];
                string author = authors[I];
                string title = titles[I];
                string path = paths[I];
                title = title.Replace("\\'", "'").Replace("<", "").Replace(">", "").Replace(":", " ").Replace("\"", "").Replace("/", "").Replace("\\", "").Replace("|", "").Replace("?", "").Replace("*", "");
                if (author.Length+title.Length > 226)
                    title = title.Remove(226-author.Length);
                author = author.Replace(":", " "); // : is illegal in Windows directory name
                path = "texts\\\\" + author + "\\\\" + title + ".txt";
                if (File.Exists("J:\\caches\\" + path))
                {
                    var updatecmd = new MySqlCommand("update sources set path=\"" + path + "\" where etext=\"" + etext + "\"", updateConnection);
                    var numRows = updatecmd.ExecuteNonQuery();
                }
            }
            updateConnection.Close();
            Console.WriteLine("Incorrect paths={0} corrected paths={1} out of {2}", incorrectPaths,correctedPaths,incorrectPaths+correctPaths+correctedPaths);
        }

        // move F:\lp\gutenbergMirror\1155-0.txt to
        // J:\caches\texts\Christie Agatha\Secret Adversary.txt
        static int moveAndRename(string etextNum, string path, string title, string creator)
        {
            string oldPath = "F:\\lp\\gutenbergMirror\\" + path;
            title = title.Replace("\\'", "'");
            string newPath = "J:\\caches\\texts\\" + creator + "\\" + title + ".txt";
            try
            {
                if (!Directory.Exists("J:\\caches\\texts\\" + creator))
                    Directory.CreateDirectory("J:\\caches\\texts\\" + creator);
                if (File.Exists(oldPath) && !File.Exists(newPath))
                {
                    if (!test)
                        File.Move(oldPath, newPath);
                    //Console.WriteLine("path {0} moved to path {1}.", oldPath, newPath);
                }
                else if (File.Exists(newPath))
                {
                    //Console.WriteLine("path {0} already moved.", oldPath);
                    return 1;
                }
                else
                {
                    Console.WriteLine("Error: path {0} not found for title {1} creator {2} etext={3}.", oldPath,title,creator,etextNum);
                    return -1;
                }
                return 0;
            }
            catch (Exception e)
            {
                Console.WriteLine("Failed to move file: {0} to {1} - {2}", oldPath, newPath, e.ToString());
                return -1;
            }
        }

        // fields to fill:
        // sourcetype=2
        // etext=1155
        // path
        //  <dcterms:hasFormat>
        //**    <pgterms:file rdf:about = "http://www.gutenberg.org/files/1155/1155-0.txt">
        //**        <dcterms:isFormatOf rdf:resource = "ebooks/1155"/>
        //          <dcterms:extent rdf:datatype = "http://www.w3.org/2001/XMLSchema#integer" > 473691 </ dcterms:extent>
        //          <dcterms:modified rdf:datatype = "http://www.w3.org/2001/XMLSchema#dateTime" > 2016 - 11 - 03T07: 27:06 </dcterms:modified>
        //          <dcterms:format>
        //              <rdf:Description rdf:nodeID = "N2ca85c92d27d466d9ec2c87234dcdc5d">
        //                  <rdf:value rdf:datatype = "http://purl.org/dc/terms/IMT" > text/plain; charset=utf-8 </rdf:value>
        //                  <dcam:memberOf rdf:resource = "http://purl.org/dc/terms/IMT"/>
        //              </rdf:Description>
        //          </dcterms:format>
        //      </pgterms:file>
        //  </dcterms:hasFormat>
        // start - ?
        // repeatStart = 1
        // author - Christie Agatha 1890-1976
        //  <dcterms:creator>
        //    <pgterms:agent rdf:about = "2009/agents/451">
        //        <pgterms:birthdate rdf:datatype = "http://www.w3.org/2001/XMLSchema#integer" > 1890 </pgterms:birthdate>
        //        <pgterms:webpage rdf:resource = "http://en.wikipedia.org/wiki/Agatha_Christie"/>
        //        <pgterms:alias > Christie, Agatha Mary Clarissa Miller</ pgterms:alias>
        //**      <pgterms:name > Christie, Agatha </ pgterms:name>
        //        <pgterms:deathdate rdf:datatype = "http://www.w3.org/2001/XMLSchema#integer" > 1976 </pgterms:deathdate>
        //    </pgterms:agent >
        //   </dcterms:creator >
        // title
        // <dcterms:title>The Secret Adversary</dcterms:title>
        // date - date issued - 1998-01-01 00:00:00
        // <dcterms:issued rdf:datatype="http://www.w3.org/2001/XMLSchema#date">1998-01-01</dcterms:issued>
        static void Main(string[] args)
        {
            checkExistingSources();
            MySqlConnection insertConnection = new MySqlConnection(string.Format("Server=localhost; database={0}; UID={1}; password={2}", "lp", "root", "byron0"));
            insertConnection.Open();
            var bookTypes =new SortedDictionary<string, int>();
            int numInsertableBooks = 0, numBooksAlreadyExist = 0, numBooksWrongType = 0, filesNotFound=0, numBooksAuthorsNotFound=0,rdfFilesProcessed=0, numBooksTitleNotFound=0;
            int numAcceptableBookTypes = 0, numReadmeBooks=0;
            string[] acceptableBookTypes = new string[] { "PC", "PE", "PN", "PR", "PS", "PZ", "PQ", "PT","PG","PH","PK","D","DG","DT","DA","DQ","DU","SK","QL" };
            HashSet<string> unknownMarcrelRelators = new HashSet<string>();
            HashSet<string> etexts = getExistingSources();
            foreach (string filepath in System.IO.Directory.GetFiles("F:\\lp\\gutenbergMirrorRDFs", "*.rdf"))
            {
                rdfFilesProcessed++;
                Console.Title = rdfFilesProcessed + " RDFs processed.";
                using (XmlReader reader = XmlReader.Create(filepath, new XmlReaderSettings { DtdProcessing = DtdProcessing.Parse }))
                {
                    // move the reader to the start of the content and read the root element's start tag
                    //   that is, the reader is positioned at the first child of the root element
                    reader.MoveToContent();
                    reader.ReadStartElement("RDF", rdf.NamespaceName);
                    int numBookElements = 0;
                    foreach (XElement etext in ReadElements(reader, pgterms + "ebook"))
                    {
                        numBookElements++;
                        string etextNum = (string)etext.Attribute(rdf + "about");
                        etextNum = etextNum.Replace("ebooks/", "");
                        bool alreadyExists = etexts.Contains(etextNum);
                        string path = "";
                        string dataType = "";
                        foreach (XElement formats in etext.Descendants(dcterms + "hasFormat"))
                        {
                            foreach (XElement pathElement in formats.Elements(pgterms + "file"))
                            {
                                foreach (XElement dataTypeElement in pathElement.Descendants(rdf + "value"))
                                    dataType = dataTypeElement.Value;
                                if (dataType.Contains("text/plain"))
                                {
                                    path = (string)pathElement.Attribute(rdf + "about");
                                    int where = path.LastIndexOf('/');
                                    if (where >= 0)
                                        path = path.Substring(where + 1);
                                    break;
                                }
                            }
                            if (dataType.Contains("text/plain") && File.Exists("F:\\lp\\gutenbergMirror\\" + path) && !path.Contains("readme.")) 
                                break;
                        }
                        string title = "";
                        if (etext.Element(dcterms + "title")==null)
                        {
                            //Console.WriteLine("Title not found in {0}",filepath);
                            numBooksTitleNotFound++;
                        }
                        else
                            title = etext.Element(dcterms + "title").Value.Replace("\n"," ").Replace("\r"," ");
                        title = title.Replace("\\'", "'").Replace("<", "").Replace(">", "").Replace(":", " ").Replace("\"", "").Replace("/", "").Replace("\\", "").Replace("|", "").Replace("?", "").Replace("*", "");
                        if (title.Length > 183)
                            title = title.Remove(183);
                        string creator = null;
                        if (etext.Element(dcterms + "creator") != null)
                        {
                            XElement creatorContainerElement = etext.Element(dcterms + "creator");
                            if (creatorContainerElement != null)
                            {
                                XElement agentElement = creatorContainerElement.Element(pgterms + "agent");
                                if (agentElement != null)
                                {
                                    XElement creatorElement = agentElement.Element(pgterms + "name");
                                    if (creatorElement != null)
                                        creator = creatorElement.Value;
                                }
                            }
                        }
                        string[] marcrelRelators = new string[] { "edt", "lbt", "prf", "cmp", "arr", "unk", "oth", "com", "ctb", "trl", "ill", "aui", "pbl", "cmm", "ann", "dub", "trc", "prt", "adp", "egr", "pht", "art" };
                        foreach (string mr in marcrelRelators)
                        {
                            if (etext.Element(marcrel + mr) != null)
                            {
                                XElement marcrelContainerElement = etext.Element(marcrel + mr);
                                if (marcrelContainerElement != null)
                                {
                                    XElement agentElement = marcrelContainerElement.Element(pgterms + "agent");
                                    if (agentElement != null)
                                    {
                                        XElement creatorElement = agentElement.Element(pgterms + "name");
                                        if (creatorElement != null)
                                            creator = creatorElement.Value;
                                    }
                                }
                            }
                        }
                        // readme are files accompanying other non-text files, so they should be ignored.
                        if (path.Contains("readme."))
                        {
                            if (alreadyExists)
                                Console.WriteLine("Already existing etext {0} title {1} by {2} in path {3} is a readme", etextNum, title, creator, path);
                            numReadmeBooks++;
                            continue;
                        }
                        if (creator is null)
                        {
                            if (alreadyExists)
                                Console.WriteLine("Author not found in {0}",filepath);
                            string estr = etext.ToString();
                            int where=etext.ToString().IndexOf(marcrel.ToString());
                            if (where>=0)
                            {
                                //Console.WriteLine("Author found in {0}.", etext.ToString().Substring(where - 11, 4));
                                unknownMarcrelRelators.Add(etext.ToString().Substring(where - 11, 4));
                            }
                            creator = "not found";
                            numBooksAuthorsNotFound++;
                        }
                        creator=creator.Replace(":", " "); // : is illegal in Windows directory name
                        string dateIssued = etext.Element(dcterms + "issued").Value;
                        string bookType = null, language = null;
                        Boolean atLeastOneLCC = false;
                        foreach (XElement subjectElement in etext.Descendants(dcterms + "subject"))
                        {
                            XElement descriptionElement = subjectElement.Element(rdf + "Description");
                            if (descriptionElement != null)
                                foreach (XElement LCCElement in descriptionElement.Elements(dcam + "memberOf"))
                                {
                                    if (LCCElement.Attribute(rdf + "resource")!=null && LCCElement.Attribute(rdf + "resource").Value == "http://purl.org/dc/terms/LCC")
                                        atLeastOneLCC = true;
                                }
                            if (atLeastOneLCC)
                            {
                                foreach (XElement LCCValue in descriptionElement.Elements(rdf + "value"))
                                    bookType = LCCValue.Value;
                                break;
                            }
                        }
                        foreach (XElement languageElement in etext.Descendants(dcterms + "language"))
                        {
                            XElement descriptionElement = languageElement.Element(rdf + "Description");
                            if (descriptionElement!=null)
                                foreach (XElement languageType in descriptionElement.Elements(rdf + "value"))
                                    language = languageType.Value;
                        }
                        if ((atLeastOneLCC) && acceptableBookTypes.Any(s => s == bookType) && 
                             (language == null || language == "en"))
                        {
                            numAcceptableBookTypes++;
                            title = title.Replace("'", "\\'");
                            if (!alreadyExists)
                            {
                                string insert = "INSERT INTO sources (sourceType,etext,path,start,repeatStart,author,title,date) VALUES (2,";
                                insert += "\""+etextNum + "\",";
                                // 'texts\Alfred Gatty\Aunt Judy''s Tales.txt'
                                insert += "\"texts\\"+creator+"\\"+title + ".txt\",";
                                insert += "\"**FIND**\",";
                                insert += "1,";
                                insert += "\""+ creator + "\",";
                                insert += "\"" + title + "\",";
                                insert += "\"" + dateIssued + "\")";
                                var cmd = new MySqlCommand(insert, insertConnection);
                                var numRows = 0;
                                if (!test)
                                {
                                    try {
                                        numRows = cmd.ExecuteNonQuery();
                                    }
                                    catch (Exception ex)
                                    {
                                        Console.WriteLine(ex.ToString());
                                    }
                                }
                                numInsertableBooks++;
                                //Console.WriteLine("Title {0} by {1} in path {2} written to database ({3}) [{4}].", title, creator, path, numRows, filepath);
                                if (moveAndRename(etextNum, path, title, creator) < 0)
                                    filesNotFound++;
                            }
                            else
                            {
                                //Console.WriteLine("Title {0} by {1} in path {2} already exists in database.", title, creator, path);
                                moveAndRename(etextNum, path, title, creator);
                                numBooksAlreadyExist++;
                            }
                        }
                        else
                        {
                            numBooksWrongType++;
                            if (alreadyExists)
                                Console.WriteLine("Already existing title {0} by {1} in path {2} has the wrong type - {3} {4}.", title.Replace("\n"," "), creator, path, bookType, language);
                            if (bookType is null)
                                bookType = "!";
                            if (bookTypes.ContainsKey(bookType))
                                bookTypes[bookType]++;
                            else
                                bookTypes[bookType] = 0;
                        }
                    }
                    if (numBookElements!=1)
                        Console.WriteLine("Scanning {0} found {1} books...", filepath, numBookElements);
                }
            }
            insertConnection.Close();
            var generalBookTypes = new SortedDictionary<string, int>();
            foreach (KeyValuePair<string, int> entry in bookTypes)
            {
                string key = "";
                switch (entry.Key[0])
                {
                    // from http://www.loc.gov/catdir/cpso/lcco/
                    case 'A': key = "GENERAL WORKS"; break;
                    case 'B': key = "PHILOSOPHY.PSYCHOLOGY.RELIGION"; break;
                    case 'C': key = "AUXILIARY SCIENCES OF HISTORY"; break;
                    case 'D': key = "WORLD HISTORY AND HISTORY OF EUROPE, ASIA, AFRICA, AUSTRALIA, NEW ZEALAND, ETC."; break;
                    case 'E': key = "HISTORY OF THE AMERICAS"; break;
                    case 'F': key = "HISTORY OF THE AMERICAS"; break;
                    case 'G': key = "GEOGRAPHY.ANTHROPOLOGY.RECREATION"; break;
                    case 'H': key = "SOCIAL SCIENCES"; break;
                    case 'J': key = "POLITICAL SCIENCE"; break;
                    case 'K': key = "LAW"; break;
                    case 'L': key = "EDUCATION"; break;
                    case 'M': key = "MUSIC AND BOOKS ON MUSIC"; break;
                    case 'N': key = "FINE ARTS"; break;
                    case 'P': key = "LANGUAGE AND LITERATURE"; break;
                    case 'Q': key = "SCIENCE"; break;
                    case 'R': key = "MEDICINE"; break;
                    case 'S': key = "AGRICULTURE"; break;
                    case 'T': key = "TECHNOLOGY"; break;
                    case 'U': key = "MILITARY SCIENCE"; break;
                    case 'V': key = "NAVAL SCIENCE"; break;
                    case 'Z': key = "BIBLIOGRAPHY.LIBRARY SCIENCE.INFORMATION RESOURCES(GENERAL)"; break;
                    case '!': key = "NOT FOUND"; break;
                    default: key = "" + entry.Key; break;
                }
                if (generalBookTypes.ContainsKey(key))
                    generalBookTypes[key] += entry.Value;
                else
                    generalBookTypes[key] = entry.Value;
            }
            Console.WriteLine("{0} RDF total: {1} acceptable books ({2} new, {3} books already existing in database), {4} books of the wrong type {5} readmes (ignored)",
                rdfFilesProcessed, numAcceptableBookTypes, numInsertableBooks, numBooksAlreadyExist, numBooksWrongType,numReadmeBooks);
            Console.WriteLine("{0} files not found {1} authors not found {2} title not found",filesNotFound, numBooksAuthorsNotFound, numBooksTitleNotFound);
            foreach (string m in unknownMarcrelRelators)
            {
                Console.WriteLine("marcel {0}", m);
            }
            foreach (KeyValuePair<string, int> entry in generalBookTypes)
            {
                Console.WriteLine("{0}:{1}", entry.Key, entry.Value);
            }
        }
    }
}
