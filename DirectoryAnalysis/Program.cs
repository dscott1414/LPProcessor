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
        private static void ShowAllFoldersUnder(string path, int indent)
        {
            try
            {
                if ((File.GetAttributes(path) & FileAttributes.ReparsePoint)
                    != FileAttributes.ReparsePoint)
                {
                    foreach (string folder in Directory.GetDirectories(path))
                    {
                        int fileCount = Directory.GetFiles(path, "*.*", SearchOption.TopDirectoryOnly).Length;
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

        static void Main(string[] args)
        {
            ShowAllFoldersUnder("J:\\caches\\dbpediacache", 0);
        }
    }
}
