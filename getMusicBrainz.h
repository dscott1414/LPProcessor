#pragma once
// Batch B2: this header uses lpchar_t/lpwstring/lp_* directly but (like most headers
// in this codebase, which historically relied on wchar_t/wstring needing zero project-
// specific include) does not include its own dependencies -- self-sufficient fix, same
// reasoning as logging.h (see its own comment) rather than trusting caller include order.
#include "lpchar.h"
/*
	getMusicBrainz.h - MusicBrainz WS/2 result structs and lookup entry points

	Overview:
		Thin data model for MusicBrainz XML search hits (release / recording / artist /
		label). The corresponding .cpp fetches https://www.musicbrainz.org/ws/2/... and
		walks tinyxml2 trees into these structs, then question-answering binds them to
		in-source objects.

	Pipeline position:
		On-demand during question answering when a query mentions artist/label/release
		roles. Not part of novel-parse initialization.

	Key entry points:
		- getReleases / getRecordings / getArtists / getLabels - fill the out-vector from
		  a "byWhatType=what" query (e.g. artist=Jay-Z)

	Notes / gotchas:
		filterNameDuplicates defaults to false (keep every hit) so the handful of existing
		3-arg call sites (main.cpp) keep compiling and keep their original unfiltered
		behavior; pass true explicitly to drop adjacent same-name hits (see absorbReleases
		in the .cpp).
*/
// https://www.musicbrainz.org/ws/2/release/?query=artist:Jay-Z
// One MusicBrainz release (album) hit plus its artist/label/group metadata.
typedef struct 
{
	lpwstring releaseId;
	lpwstring title;
	lpwstring status;
	lpwstring artistId;
	lpwstring artistName;
	lpwstring releaseGroupId;
	lpwstring releaseGroupType;
	lpwstring date;
	lpwstring country;
	lpwstring labelName;
	lpwstring labelId;
} mbInfoReleaseType;

// One recording (track) plus the releases it appears on.
typedef struct 
{
	lpwstring recordingId;
	lpwstring title;
	lpwstring artistId;
	lpwstring artistName;
	vector <mbInfoReleaseType> releases;
} mbInfoRecordingType;

// One artist hit plus alias strings from the alias-list.
typedef struct 
{
	lpwstring artistType;
	lpwstring artistId;
	lpwstring artistName;
	vector <lpwstring> aliases;
} mbInfoArtistType;

// One label hit plus alias strings from the alias-list.
typedef struct 
{
	lpwstring labelType;
	lpwstring labelId;
	lpwstring labelName;
	vector <lpwstring> aliases;
} mbInfoLabelType;

// Query WS/2 for releases matching byWhatType:what; appends into mbTypes. Returns 0.
int getReleases(lpwstring byWhatType,lpwstring what,vector <mbInfoReleaseType> &mbTypes, bool filterNameDuplicates = false);
// Query WS/2 for recordings matching byWhatType:what; appends into mbTypes. Returns 0.
// Query WS/2 for artists matching byWhatType:what; appends into mbTypes. Returns 0.
// Query WS/2 for labels matching byWhatType:what; appends into mbTypes. Returns 0.
