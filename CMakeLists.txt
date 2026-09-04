# Top-level macOS build for lp / CorpusAnalysis (formerly built only via
# lp.vcxproj / specials.vcxproj on Windows/MSVC). See the Mac-port plan for
# the full design rationale; this file implements batch B0 (skeleton) and
# will be filled in progressively by later batches (B1-B15) as the ported
# sources become compilable. Until then, `lpcore`/`lp`/`CorpusAnalysis` are
# EXPECTED to fail to compile — that failure is the ground truth later
# batches work against, not a bug in this file.
#
# NOTE: never add file(GLOB ...) anywhere in this file. `source/` sits next
# to Gutenberg/ (65,535 subdirectories) and other huge non-code trees; every
# source list here is an explicit, transcribed-from-.vcxproj file list.

cmake_minimum_required(VERSION 3.20)
project(lp LANGUAGES CXX C)

# Batch B15: pin the architecture. Every dependency this build links against is
# arm64 only -- the Homebrew MySQL client, the Temurin 21 JVM, the system libcurl --
# so an x86_64 build cannot link at all, and the failure surfaces as a wall of
# "Undefined symbols for architecture x86_64" that says nothing about the real
# cause. Being explicit here also stops the build from silently following whatever
# architecture the invoking shell happens to be running under (a Rosetta terminal
# produced exactly that x86_64 build once).
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
  set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "Build architecture" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(BUILD_SHARED_LIBS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra)
# Matches the existing MSVC philosophy (EnableAllWarnings, TreatWarningAsError=false):
# surface warnings, do not block the build on them.

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Vendored CMakeLists.txt files here say `cmake_minimum_required(VERSION 2.6)`;
# harmless deprecation warning on our CMake 3.30, but would hard-fail on
# CMake >=4.0. Cheap insurance, matches upstream's own intent.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# ---------------------------------------------------------------------------
# yajl (JSON) — vendored source at lloyd-yajl-66cb08c/. The top-level dir is
# NOT source, it is pure Windows build OUTPUT and is ignored here.
# ---------------------------------------------------------------------------
add_subdirectory(lloyd-yajl-66cb08c EXCLUDE_FROM_ALL)
# EXCLUDE_FROM_ALL: don't build yajl's own test/example/perf/doc targets by
# default; target_link_libraries(... yajl_s) below still pulls in yajl_s
# specifically regardless of this flag.

# lloyd-yajl-66cb08c/src/api/yajl_version.h is committed as the raw
# .cmake template (literal "${YAJL_MAJOR}" placeholders, never expanded) —
# relying on yajl's own POST_BUILD header-copy step would race against
# lpcore's own compilation (link-time dependency != compile-time header
# availability). Generate a correct one ourselves at configure time instead,
# which runs before anything is compiled.
set(YAJL_MAJOR 2)
set(YAJL_MINOR 1)
set(YAJL_MICRO 0)
set(YAJL_GENERATED_INCLUDE_DIR ${CMAKE_BINARY_DIR}/yajl_generated_include)
file(MAKE_DIRECTORY ${YAJL_GENERATED_INCLUDE_DIR})
configure_file(
  ${CMAKE_SOURCE_DIR}/lloyd-yajl-66cb08c/src/api/yajl_version.h
  ${YAJL_GENERATED_INCLUDE_DIR}/yajl_version.h
)
# @ONLY would normally restrict substitution to @VAR@ form only, but the
# template uses ${VAR} form; configure_file() substitutes both forms unless
# @ONLY is given, so @ONLY is deliberately OMITTED here — do not add it.

# yajl's OWN internal sources (yajl.c etc., in src/) #include <yajl_common.h>
# etc. via angle brackets, expecting them findable on the include path —
# but src/CMakeLists.txt only wires that up via a POST_BUILD copy step tied
# to the yajl_s target itself, which hasn't run yet the first time yajl_s's
# own sources are compiled (a real ordering bug in the vendored build
# script, not something we're choosing to work around cleverly). Augment
# the already-defined yajl_s/yajl targets from here instead of editing the
# vendored CMakeLists.txt.
target_include_directories(yajl_s PUBLIC
  ${YAJL_GENERATED_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/lloyd-yajl-66cb08c/src/api
)
if(TARGET yajl)
  target_include_directories(yajl PUBLIC
    ${YAJL_GENERATED_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/lloyd-yajl-66cb08c/src/api
  )
endif()

# ---------------------------------------------------------------------------
# tinyxml2 — vendored at tinyxml2-master/. On Windows this is compiled
# directly as one more file of lp.vcxproj/specials.vcxproj (not a separate
# lib) — lpcore's own source list below includes tinyxml2-master/tinyxml2.cpp
# directly, matching that. Only its include dir is needed here.
# ---------------------------------------------------------------------------
# (tinyxml2-master/tinyxml2.cpp is compiled as part of lpcore's SOURCES list
# below; no add_subdirectory/add_library needed for it.)

# ---------------------------------------------------------------------------
# WordNet 2.1 — the small portable C wrapper library, vendored OUTSIDE this
# git repo at ../wn/ (sibling to source/), no CMakeLists.txt of its own.
# Headers/dict data from the vendored ../WordNet/2.1/ tree (NOT a Homebrew
# formula — this is a specific, tightly-coupled WordNet 2.1 data layout).
# wn/'s Win32 registry lookups (morph.c/search.c/wnutil.c) are entirely
# inside #ifdef _WINDOWS with a working POSIX #else branch already
# (getenv("WNSEARCHDIR")/getenv("WNHOME")/DEFAULTPATH) — we simply never
# define _WINDOWS, so no source change is needed for that; the 3 raw
# `#include <windows.h>`/`<windowsx.h>` lines still need guarding behind
# `#ifdef _WIN32` before this will compile (tracked as a small fixup, not
# done in this skeleton batch).
# ---------------------------------------------------------------------------
set(WN_DIR ${CMAKE_SOURCE_DIR}/../wn)
set(WORDNET_DIR ${CMAKE_SOURCE_DIR}/../WordNet/2.1)
if(EXISTS ${WN_DIR}/wnutil.c AND EXISTS ${WORDNET_DIR}/src/include/wn.h)
  add_library(wn STATIC
    ${WN_DIR}/binsrch.c
    ${WN_DIR}/morph.c
    ${WN_DIR}/search.c
    ${WN_DIR}/wnglobal.c
    ${WN_DIR}/wnhelp.c
    ${WN_DIR}/wnrtl.c
    ${WN_DIR}/wnutil.c
  )
  target_include_directories(wn PUBLIC ${WORDNET_DIR}/src/include)
  set(LP_HAVE_WN TRUE)
else()
  message(WARNING "wn/ or WordNet/2.1/ not found next to source/ — lpcore will not link until this is resolved")
  set(LP_HAVE_WN FALSE)
endif()

# ---------------------------------------------------------------------------
# MySQL Connector/C — external, not vendored. RESOLVED in batch B3: the
# `brew install mysql-client` the port plan proposed as a B15 fallback
# succeeded once the Xcode CLT problem was fixed, so headers and libs are now
# at /opt/homebrew/opt/mysql-client (Connector/C 26.7.0), which the search
# paths below already cover. Note the version gap: this code was written
# against a much older Connector/C, so API drift (my_bool, removed
# mysql_options codes, ...) is a real possibility for batch B5 to find.
# Still NOT required at configure time — a machine without it should be able
# to configure and build everything that does not touch the DB.
# ---------------------------------------------------------------------------
find_package(PkgConfig QUIET)
set(LP_HAVE_MYSQL FALSE)
if(PkgConfig_FOUND)
  pkg_check_modules(MYSQL QUIET mysqlclient)
  if(MYSQL_FOUND)
    set(LP_HAVE_MYSQL TRUE)
  endif()
endif()
if(NOT LP_HAVE_MYSQL)
  find_path(MYSQL_INCLUDE_DIR NAMES mysql.h
    PATHS /usr/local/opt/mysql-client/include/mysql /usr/local/opt/mysql/include/mysql
          /opt/homebrew/opt/mysql-client/include/mysql /opt/homebrew/opt/mysql/include/mysql)
  find_library(MYSQL_LIBRARY NAMES mysqlclient
    PATHS /usr/local/opt/mysql-client/lib /usr/local/opt/mysql/lib
          /opt/homebrew/opt/mysql-client/lib /opt/homebrew/opt/mysql/lib)
  if(MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)
    set(LP_HAVE_MYSQL TRUE)
    set(MYSQL_INCLUDE_DIRS ${MYSQL_INCLUDE_DIR})
    set(MYSQL_LIBRARIES ${MYSQL_LIBRARY})
  endif()
endif()
if(NOT LP_HAVE_MYSQL)
  message(WARNING "MySQL client library not found. On this machine it comes from "
                   "`brew install mysql-client`; lpcore files including mysql.h cannot compile without it.")
endif()

# ---------------------------------------------------------------------------
# JNI — pinned to the Temurin 21 (arm64) JDK specifically; two other JDKs on
# this machine are x86_64-only Java 8 and would fail to link against an
# arm64 target.
# ---------------------------------------------------------------------------
if(NOT DEFINED ENV{JAVA_HOME})
  execute_process(
    COMMAND /usr/libexec/java_home -v 21
    OUTPUT_VARIABLE LP_JAVA_HOME
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  if(LP_JAVA_HOME)
    set(ENV{JAVA_HOME} ${LP_JAVA_HOME})
  endif()
endif()
find_package(JNI QUIET)
if(NOT JNI_FOUND)
  message(WARNING "JNI not found — set JAVA_HOME to the Temurin 21 (arm64) JDK, e.g. "
                   "export JAVA_HOME=$(/usr/libexec/java_home -v 21)")
endif()

# ---------------------------------------------------------------------------
# libcurl — replacement for WinINet/WinHTTP (Internet.cpp, batch B9).
# System libcurl on macOS is sufficient (confirmed present, 8.7.1).
# ---------------------------------------------------------------------------
find_package(CURL QUIET)
if(NOT CURL_FOUND)
  message(WARNING "libcurl not found via find_package(CURL) — unexpected on macOS, investigate before B9")
endif()

# ---------------------------------------------------------------------------
# lpcore — the 48 .cpp files confirmed (by diffing lp.vcxproj's and
# specials.vcxproj's actual <ClCompile> lists) to be shared identically by
# both binaries. This *improves* on the Windows build, which compiles these
# 48 files twice (once per project); here they compile once and link twice.
# ---------------------------------------------------------------------------
set(LPCORE_SOURCES
  DB.cpp
  DBCreateSQLSchema.cpp
  DBUtility.cpp
  DBWordRelations.cpp
  Internet.cpp
  agreement.cpp
  conversationContext.cpp
  createOntology.cpp
  dbQuerySearch.cpp
  definePatterns.cpp
  envConfig.cpp
  getDictionary.cpp
  getMusicBrainz.cpp
  getThesaurus.cpp
  getTwitter.cpp
  getWikipedia.cpp
  getWordNet.cpp
  getWordNetMaps.cpp
  hmm.cpp
  identifyObjects.cpp
  identifySpeakerGroups.cpp
  initializeDictionary.cpp
  logging.cpp
  lpFile.cpp
  lpProcess.cpp
  lpchar.cpp
  memoryStat.cpp
  names.cpp
  paice.cpp
  pattern.cpp
  patternElementMatchArray.cpp
  patternMatchArray.cpp
  questionAnswering.cpp
  questionAnsweringWebSearch.cpp
  questionProcessing.cpp
  resolveFirstSecondPersonPronouns.cpp
  resolveMetaGroupObjects.cpp
  resolveObjects.cpp
  resolveSpeakers.cpp
  semanticRelations.cpp
  source.cpp
  syntacticRelationGroups.cpp
  syntacticRelations.cpp
  tableColumn.cpp
  tagOperations.cpp
  timeRelations.cpp
  "tinyxml2-master/tinyxml2.cpp"
  tokenize.cpp
  utfConvert.cpp
  utilities.cpp
  vcXML.cpp
  word.cpp
)
# lpchar.cpp/utfConvert.cpp (batch B1) are listed here per the port plan's own
# instruction even though nothing in lpcore calls them yet (that starts in B2) --
# they compile cleanly and standalone (no windows.h/mysql.h/etc. dependency), so
# their presence here cannot newly break lpcore beyond its already-expected-broken
# state (see the file header). Correctness is proven independently by the
# lpchar_smoketest target further down, which does NOT depend on lpcore.

add_library(lpcore STATIC ${LPCORE_SOURCES})
target_include_directories(lpcore PUBLIC
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/tinyxml2-master
  ${CMAKE_SOURCE_DIR}/lloyd-yajl-66cb08c/src/api
  ${YAJL_GENERATED_INCLUDE_DIR}
)
target_link_libraries(lpcore PUBLIC yajl_s)

if(LP_HAVE_WN)
  target_link_libraries(lpcore PUBLIC wn)
endif()
if(LP_HAVE_MYSQL)
  target_include_directories(lpcore PUBLIC ${MYSQL_INCLUDE_DIRS})
  target_link_libraries(lpcore PUBLIC ${MYSQL_LIBRARIES})
endif()
if(JNI_FOUND)
  target_include_directories(lpcore PUBLIC ${JNI_INCLUDE_DIRS})
  target_link_libraries(lpcore PUBLIC ${JNI_LIBRARIES})
endif()
if(CURL_FOUND)
  target_link_libraries(lpcore PUBLIC CURL::libcurl)
endif()

# Main-thread stack. The Windows build reserved ~20.1 MiB
# (StackReserveSize=21097152, identical on every x64 config of both .vcxproj
# files) solely to accommodate multi-megabyte stack arrays: two at 19 MiB, three
# at 10 MiB, one at 1.9 MiB. All of those are heap-allocated now (see
# cTrackedBuffer in general.h), and the largest array left anywhere in the tree
# is 100 KiB, so the oversized reservation is no longer needed -- reclaiming it
# was the actual payoff PROPOSED_FIXES.md identified, not crash avoidance.
#
# 8 MiB (0x800000) is macOS's own default main-thread stack, kept explicit here
# so the value is a decision rather than an accident. That is 80x the largest
# remaining buffer, ample for the parser's recursion. If a deep parse ever does
# overflow it, this is the line to raise -- and the crash will be an obvious
# SIGSEGV in a deeply recursive frame, not silent corruption.
set(LP_STACK_SIZE_FLAG "-Wl,-stack_size,0x800000")

add_executable(lp main.cpp "Adversary bugs.cpp")
target_link_libraries(lp PRIVATE lpcore)
target_link_options(lp PRIVATE ${LP_STACK_SIZE_FLAG})

add_executable(CorpusAnalysis specials_main.cpp)
target_link_libraries(CorpusAnalysis PRIVATE lpcore)
target_link_options(CorpusAnalysis PRIVATE ${LP_STACK_SIZE_FLAG})

# ---------------------------------------------------------------------------
# B0 toolchain smoke test — deliberately independent of the 48-file lpcore
# list above (which is expected to fail to compile until later batches land
# the char16_t/Win32 port). Proves the environment itself (compiler, CMake,
# JNI/CURL discovery) works end to end. Build with:
#   cmake --build <builddir> --target lp_smoketest
# ---------------------------------------------------------------------------
add_executable(lp_smoketest EXCLUDE_FROM_ALL cmake_smoketest.cpp)
if(CURL_FOUND)
  target_link_libraries(lp_smoketest PRIVATE CURL::libcurl)
  target_compile_definitions(lp_smoketest PRIVATE LP_SMOKETEST_HAVE_CURL=1)
endif()
if(JNI_FOUND)
  target_include_directories(lp_smoketest PRIVATE ${JNI_INCLUDE_DIRS})
  target_compile_definitions(lp_smoketest PRIVATE LP_SMOKETEST_HAVE_JNI=1)
endif()
target_link_libraries(lp_smoketest PRIVATE yajl_s)

# ---------------------------------------------------------------------------
# B1 lpchar/utfConvert smoke test — deliberately independent of the lpcore
# target (which is not expected to build until later batches land the
# char16_t/Win32 port). Compiles lpchar.cpp/utfConvert.cpp directly alongside
# its own throwaway main(), proving batch B1's printf engine, UTF-8<->UTF-16
# codec, and string primitives are correct in isolation via real compiled-and-
# run assertions (see lpchar_smoketest.cpp). Build and run with:
#   cmake --build <builddir> --target lpchar_smoketest
#   <builddir>/bin/lpchar_smoketest
# ---------------------------------------------------------------------------
add_executable(lpchar_smoketest EXCLUDE_FROM_ALL
  lpchar_smoketest.cpp
  lpchar.cpp
  utfConvert.cpp
)
target_include_directories(lpchar_smoketest PRIVATE ${CMAKE_SOURCE_DIR})

# ---------------------------------------------------------------------------
# B4a lpProcess smoke test — same pattern as lpchar_smoketest: independent of
# lpcore, so batch B4a's process/signal/progress layer (posix_spawn, waitpid,
# SIGINT delivery, sibling-executable discovery, progress-title sanitizing) is
# proven by real spawned-and-reaped processes long before the engine links.
# Build and run with:
#   cmake --build <builddir> --target lpprocess_smoketest
#   <builddir>/bin/lpprocess_smoketest
# ---------------------------------------------------------------------------
add_executable(lpprocess_smoketest EXCLUDE_FROM_ALL
  lpprocess_smoketest.cpp
  lpProcess.cpp
  lpchar.cpp
  utfConvert.cpp
)
target_include_directories(lpprocess_smoketest PRIVATE ${CMAKE_SOURCE_DIR})

message(STATUS "lp Mac port — through batch B3 (root headers)")
message(STATUS "  LP_HAVE_WN=${LP_HAVE_WN}  LP_HAVE_MYSQL=${LP_HAVE_MYSQL}  JNI_FOUND=${JNI_FOUND}  CURL_FOUND=${CURL_FOUND}")
