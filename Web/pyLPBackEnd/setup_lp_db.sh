#!/usr/bin/env bash
# Create and populate the minimal `lp` MySQL database used by the pyLPBackEnd
# Flask server (the source-search endpoints). This is idempotent: it recreates
# the `sources` table and reloads the book list every time it is run.
#
# It reproduces what the Windows C++ tool DBCreateSQLSchema.cpp does for the
# `sources` table: create the table, then load lists/bookSources.sql.
#
# Requires a running MySQL/MariaDB reachable as root/byron0 (see AGENTS.md).
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BOOK_SQL="${REPO_ROOT}/lists/bookSources.sql"

DB_HOST="${LP_DB_HOST:-127.0.0.1}"
DB_PORT="${LP_DB_PORT:-3306}"
DB_USER="${LP_DB_USER:-root}"
DB_PASS="${LP_DB_PASS:-byron0}"

mysql() { command mariadb -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" -p"$DB_PASS" "$@"; }

echo "Creating database lp (if needed)..."
mysql -e "CREATE DATABASE IF NOT EXISTS lp DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_bin;"

echo "Creating sources table..."
mysql lp <<'SQL'
DROP TABLE IF EXISTS sources;
CREATE TABLE sources (id int(11) unsigned NOT NULL auto_increment unique,
 sourceType TINYINT(4) NOT NULL,
 etext VARCHAR (10) CHARACTER SET utf8mb4,
 path VARCHAR (1024) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
 start VARCHAR(256) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
 repeatStart INT NOT NULL,
 author VARCHAR (128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin,
 title VARCHAR (1024) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin,
 date DATETIME,
 numSentences INT, matchedSentences INT, numWords INT, numUnknown INT, numUnmatched INT, numOvermatched INT,
 numQuotations INT, quotationExceptions INT, numTicks INT, numPatternMatches INT,
 sizeInBytes INT, numWordRelations INT, numMultiWordRelations INT,
 processing BIT, processed BIT,
 proc2 INT,
 duplicateId INT,
 lastProcessedTime TIMESTAMP DEFAULT 0,
 ts TIMESTAMP,
 KEY `EtextIndex` (`etext`),
 KEY `nsi` (`sourceType`,`start`,`processed`)
) DEFAULT CHARSET=utf8mb4 COLLATE utf8mb4_bin;
SQL

echo "Loading book list from ${BOOK_SQL} (UTF-16 -> UTF-8)..."
tmp_sql="$(mktemp)"
iconv -f UTF-16LE -t UTF-8 "$BOOK_SQL" -o "$tmp_sql"
mysql lp < "$tmp_sql"
rm -f "$tmp_sql"

echo -n "sources row count: "
mysql -N -e "SELECT COUNT(*) FROM lp.sources;"
echo "Done."
