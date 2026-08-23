#pragma once
/*
	getMusicBrainz.h - MusicBrainz WS/2 result structs and lookup entry points

	Overview:
		Thin data model for MusicBrainz XML search hits (release / recording / artist /
		label). The corresponding .cpp fetches http://www.musicbrainz.org/ws/2/... and
		walks tinyxml2 trees into these structs, then question-answering binds them to
		in-source objects.

	Pipeline position:
		On-demand during question answering when a query mentions artist/label/release
		roles. Not part of novel-parse initialization.

	Key entry points:
		- getReleases / getRecordings / getArtists / getLabels - fill the out-vector from
		  a "byWhatType=what" query (e.g. artist=Jay-Z)

	Notes / gotchas:
		The .cpp implementations take an extra bool filterNameDuplicates that this header
		does not declare, so callers that include only the header see a different
		signature than the definition.
*/
// http://www.musicbrainz.org/ws/2/release/?query=artist:Jay-Z
// One MusicBrainz release (album) hit plus its artist/label/group metadata.
typedef struct 
{
	wstring releaseId;
	wstring title;
	wstring status;
	wstring artistId;
	wstring artistName;
	wstring releaseGroupId;
	wstring releaseGroupType;
	wstring date;
	wstring country;
	wstring labelName;
	wstring labelId;
} mbInfoReleaseType;

// One recording (track) plus the releases it appears on.
typedef struct 
{
	wstring recordingId;
	wstring title;
	wstring artistId;
	wstring artistName;
	vector <mbInfoReleaseType> releases;
} mbInfoRecordingType;

// One artist hit plus alias strings from the alias-list.
typedef struct 
{
	wstring artistType;
	wstring artistId;
	wstring artistName;
	vector <wstring> aliases;
} mbInfoArtistType;

// One label hit plus alias strings from the alias-list.
typedef struct 
{
	wstring labelType;
	wstring labelId;
	wstring labelName;
	vector <wstring> aliases;
} mbInfoLabelType;

// Query WS/2 for releases matching byWhatType:what; appends into mbTypes. Returns 0.
int getReleases(wstring byWhatType,wstring what,vector <mbInfoReleaseType> &mbTypes);
// Query WS/2 for recordings matching byWhatType:what; appends into mbTypes. Returns 0.
int getRecordings(wstring byWhatType,wstring what,vector <mbInfoRecordingType> &mbTypes);
// Query WS/2 for artists matching byWhatType:what; appends into mbTypes. Returns 0.
int getArtists(wstring byWhatType,wstring what,vector <mbInfoArtistType> &mbTypes);
// Query WS/2 for labels matching byWhatType:what; appends into mbTypes. Returns 0.
int getLabels(wstring byWhatType,wstring what,vector <mbInfoLabelType> &mbTypes);
