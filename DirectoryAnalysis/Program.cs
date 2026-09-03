/*
	Program.cs - Recursive folder-tree reporter for a DBpedia cache

	Overview:
		Walks a hardcoded J:\caches\dbpediacache tree, printing each
		subdirectory name and the file count of its parent, while
		updating the console title with running totals. Skips reparse
		points and swallows UnauthorizedAccessException.

	Pipeline position:
		Ad-hoc cache-inventory utility; not part of the parser.

	Key entry points:
		- ShowAllFoldersUnder() - recurse one directory
		- Main() - start walk at J:\caches\dbpediacache

	Notes / gotchas:
		(fixed) fileCount used to be Directory.GetFiles(path) — the
		*parent* of `folder`, not the child being printed — so totals
		over-counted the parent's files once per sibling and the printed
		count for each folder was actually its parent's file count. Now
		Directory.GetFiles(folder, ...), matching the folder that's
		printed and recursed into. Hardcoded drive letter.
*/
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DirectoryAnalysis
{
    class Program
    {
        static int totalFolders;
        static int totalFiles;
        // Recurse into `path`, print each child folder plus the file count of
        // `path` itself (not of the child). indent is spaces of leading pad.
        // Mutates totalFolders / totalFiles. Swallows UnauthorizedAccessException.
        private static void ShowAllFoldersUnder(string path, int indent)
        {
            try
            {
                if ((File.GetAttributes(path) & FileAttributes.ReparsePoint)
                    != FileAttributes.ReparsePoint)
                {
                    foreach (string folder in Directory.GetDirectories(path))
                    {
                        int fileCount = Directory.GetFiles(folder, "*.*", SearchOption.TopDirectoryOnly).Length;
                        Console.WriteLine("{0}{1}({2})", new string(' ', indent), Path.GetFileName(folder),fileCount);
                        totalFolders++;
                        totalFiles += fileCount;
                        Console.Title = string.Format("Folders = {0},  Files = {1}", totalFolders, totalFiles);
                        ShowAllFoldersUnder(folder, indent + 2);
                    }
                }
            }
            catch (UnauthorizedAccessException) { }
        }

        // Entry: walk the DBpedia cache root. No args are consumed.
        static void Main(string[] args)
        {
            ShowAllFoldersUnder("J:\\caches\\dbpediacache", 0);
        }
    }
}
