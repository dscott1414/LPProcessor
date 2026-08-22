# AGENTS.md

## Repository overview

LPProcessor is a computational-linguistics narrative parser. The bulk of the
repository is a large **Windows-only C++/Visual Studio** program (root `*.cpp`
files, `lp.sln`, `specials.sln`) that parses novels into a MySQL `lp` database
plus on-disk caches. There are also several satellite components: a Python
Flask web backend (`Web/pyLPBackEnd`), an Angular front-end (`Web/angular-front-end`),
Java Maven tools (`show`, `TextRenderer`), .NET tools (`LPWeb`,
`DirectoryAnalysis`, `processGutenbergRDFtoSQL`), and Python/C++ HMM utilities
(`HMM`).

## Cursor Cloud specific instructions

These notes are for a Linux VM where the startup update script (Python venv +
backend dependencies) has already run. They capture the non-obvious bits;
standard commands live in each component's own README / project files.

### What runs on this Linux VM

The runnable, cross-platform product here is the **Python Flask backend**
(`Web/pyLPBackEnd/EclipseWorkspace/pyLPBackEnd/src/pyLP.py`) backed by MySQL.
Do NOT expect the main C++ parser to build/run here — it is Windows/Visual
Studio only (uses `wmain`, `SRWLOCK`, `wsprintf`, `<windows.h>`, hardcoded
`F:\lp` / `M:\caches` paths in `general.h`) and has no Makefile/CMake.

### MySQL (MariaDB) — required for the backend

- The server is installed at the system level. systemd is not running, so start
  it manually if it is not already up:
  `sudo mariadbd-safe --datadir=/var/lib/mysql &` (a `mariadb` tmux session is
  used during setup). Verify with `mariadb -h 127.0.0.1 -u root -pbyron0 -e "SELECT 1;"`.
- The backend (`pyLP.py::connect()`) hardcodes credentials
  `host=localhost port=3306 user=root password=byron0 database=lp`. The
  `root@localhost` account has been switched to `mysql_native_password` with
  password `byron0` so pymysql's TCP connection (localhost → 127.0.0.1) works.
  Because of this, `sudo mariadb` without a password no longer works — use
  `mariadb -u root -pbyron0`.
- Seed / re-seed the `lp` database with the book list any time it is empty or
  missing: `Web/pyLPBackEnd/setup_lp_db.sh`. It is idempotent, recreates the
  `sources` table (schema mirrors `DBCreateSQLSchema.cpp`) and loads
  `lists/bookSources.sql` (which is UTF-16LE and must be converted to UTF-8 —
  the script does this). Expect 783 rows afterwards.

### Running the Flask backend

- Use the venv created by the update script: from
  `Web/pyLPBackEnd/EclipseWorkspace/pyLPBackEnd/src` run
  `/workspace/.venv/bin/python pyLP.py`. It serves on `http://127.0.0.1:5000`.
- Endpoints that work on this VM (need only the `sources` table): `/` (Hello
  World), `/api/login`, `/api/searchSource`, `/api/searchAuthor`. Sessions use
  cookies, so call `/api/login` first (with a cookie jar) before session-bound
  endpoints.
- Endpoints that do NOT work here: `/api/loadSource` and everything downstream
  of it. They read binary cache files from Windows paths (`F:\lp\wordFormCache`,
  `M:\caches\*.SourceCache`) produced by the C++ parser; those caches are not in
  the repo and the paths are Windows-only.

### Angular front-end (blocked)

`Web/angular-front-end` cannot be built as-is: the repo's `.gitignore` globally
ignores `*.json` and `*.js`, so `package.json`, `angular.json`, and
`tsconfig*.json` are not committed. `npm install` / `ng serve` therefore have no
project config to work from. Only the committed `*.ts`/`*.css`/`*.html` sources
are present.

### Lint / test

There is no configured linter or runnable automated test suite for the backend
on Linux. `test.py` and `pyLP.test()` require the Windows-only cache files, and
the Angular `*.spec.ts` tests need the missing Angular config. A quick sanity
check is byte-compiling the backend:
`/workspace/.venv/bin/python -m py_compile Web/pyLPBackEnd/EclipseWorkspace/pyLPBackEnd/src/*.py`.
