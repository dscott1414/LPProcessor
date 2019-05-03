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

#include <typeinfo>
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include <set>
using namespace std;

bool Source::matchOwnershipDbQuery(wchar_t *derivation,cSpaceRelation* parentSRI)
{ LFS
	return matchOwnershipDbMusicBrainz(derivation,parentSRI);
}

bool Source::dbSearchForQuery(wchar_t *derivation,cSpaceRelation* parentSRI,vector < cAS > &answerSRIs)
{ LFS
	return dbSearchMusicBrainz(derivation,parentSRI,answerSRIs);
}