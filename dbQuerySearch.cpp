#include <stdio.h>
#include <string.h>
#include <mbstring.h>
#include <ctype.h>
#include <stdarg.h>
#include <winsock.h>
#include "Winhttp.h"
#include "io.h"
#include "word.h"
#include "mysql.h"
#include "mysqld_error.h"
#include "odbcinst.h"
#include "time.h"
#include "ontology.h"
#include "source.h"
#include <fcntl.h>
#include <sys/stat.h>
#include "profile.h"
#include "QuestionAnswering.h"

#include <typeinfo>
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include <set>
using namespace std;

bool cQuestionAnswering::matchOwnershipDbQuery(cSource* questionSource, wchar_t* derivation, cSyntacticRelationGroup* parentSRG)
{
	LFS
		return matchOwnershipDbMusicBrainz(questionSource, derivation, parentSRG);
}

bool cQuestionAnswering::dbSearchForQuery(cSource* questionSource, wchar_t* derivation, cSyntacticRelationGroup* parentSRG, vector < cAS >& answerSRGs)
{
	LFS
		return dbSearchMusicBrainz(questionSource, derivation, parentSRG, answerSRGs);
}