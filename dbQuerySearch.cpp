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

bool cQuestionAnswering::matchOwnershipDbQuery(Source *questionSource,wchar_t *derivation,cSpaceRelation* parentSRI)
{ LFS
	return matchOwnershipDbMusicBrainz(questionSource,derivation,parentSRI);
}

bool cQuestionAnswering::dbSearchForQuery(Source *questionSource, wchar_t *derivation,cSpaceRelation* parentSRI,vector < cAS > &answerSRIs)
{ LFS
	return dbSearchMusicBrainz(questionSource, derivation,parentSRI,answerSRIs);
}