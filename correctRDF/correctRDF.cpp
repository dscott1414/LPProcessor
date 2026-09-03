/*
	correctRDF.cpp - One-off DBpedia N-Triples sanitizer

	Overview:
		Standalone console tool that scans a DBpedia infobox-property-definitions
		.nt dump and writes a .clean sibling, dropping triples whose three RDF
		fields are longer than 1800 chars. Used historically to make Virtuoso
		imports survive malformed/overlong lines.

	Pipeline position:
		Offline data-prep utility; not part of the narrative-parser runtime.
		Run once against a dump, then feed the .clean file to Virtuoso.

	Key entry points:
		- _tmain() - open dump, filter triples, write .clean

	Dependencies:
		Hardcoded path G:\virtuoso_server_dumpfolder\...; MSVC _tmain / __int64.

	Notes / gotchas:
		(fixed) buf was 16348 (not 16384) — a typo vs. the intended 16 KiB
		buffer; now 16384, with the matching fgets() size argument updated
		to match.
		(fixed) valid (non-overlong) triples used to be written to .clean
		only once pastFirstError was already true, i.e. only after the
		first overlong triple had been seen - so every good triple before
		the first bad one was silently dropped from the output, the
		opposite of what the Overview above (and the rest of this tool)
		describes. pastFirstError has been removed; every triple that is
		not overlong is written to .clean regardless of file position.
		fopen failures only perror; later code guards on both FILE* being
		non-NULL.
*/
// correctRDF.cpp : Defines the entry point for the console application.
//

#define _CRT_SECURE_NO_WARNINGS
#include "stdafx.h"
#include <string.h>
#include <stdio.h>
        
//infobox_property_definitions_en.nt 
//infobox_properties_en.nt DONE
//infobox_properties_unredirected_en.nt DONE
#define rdlFile "G:\\virtuoso_server_dumpfolder\\trash_from_original\\infobox_property_definitions_en.nt"
// Filter infobox_property_definitions_en.nt into <path>.clean, skipping
// comment lines and triples whose any of the three RDF fields exceeds 1800 chars.
// Returns 0 always; fopen failures are reported via perror then skipped.
int _tmain(int argc, _TCHAR* argv[])
{
	FILE *fp=fopen(rdlFile,"r");
	if (fp==NULL) 
		perror ("Error opening file");
	char clean[1024];
	sprintf(clean,"%s.clean",rdlFile);
	remove(clean);
	FILE *fpclean=fopen(clean,"w+");
	if (fpclean==NULL) 
		perror ("Error opening file");
	if (fp && fpclean)
	{
		int linesRead=0,linesSkipped=0;
		char buf[16384];
		while (fgets(buf,16384,fp))
		{
			linesRead++;
			// just determine whether it has three elements
			// <http://dbpedia.org/property/lifeFemale> <http://www.w3.org/1999/02/22-rdf-syntax-ns#type> <http://www.w3.org/1999/02/22-rdf-syntax-ns#Property> .
			// <http://dbpedia.org/property/lifeFemale> <http://www.w3.org/2000/01/rdf-schema#label> "life female"@en .
			if (buf[0]!='#')
			{
				if (buf[0]=='<')
				{
					char *next=strchr(buf+1,'<');
					if (next)
					{
						char *next2=strchr(next+1,'<');
						if (!next2)
							next2=strchr(next+1,'\"');
						if (next2)
						{
							__int64 len1=next-buf;
							__int64 len2=next2-next;
							__int64 len3=strlen(buf)-(next2-buf);
							if (len1>1800 || len2>1800 || len3>1800)
							{
								printf("%d:%d %d %d\n",linesRead,len1,len2,len3);
								linesSkipped++;
							}
							else
								fputs(buf,fpclean);
							continue;
						}
					}
				}
				printf("error detected.");
			}
		}
		printf("linesRead=%d",linesRead);
		fclose(fp);
		fclose(fpclean);
	}
	return 0;
}

