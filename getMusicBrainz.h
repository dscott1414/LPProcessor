#pragma once
// http://www.musicbrainz.org/ws/2/release/?query=artist:Jay-Z
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

typedef struct 
{
	wstring recordingId;
	wstring title;
	wstring artistId;
	wstring artistName;
	vector <mbInfoReleaseType> releases;
} mbInfoRecordingType;

typedef struct 
{
	wstring artistType;
	wstring artistId;
	wstring artistName;
	vector <wstring> aliases;
} mbInfoArtistType;

typedef struct 
{
	wstring labelType;
	wstring labelId;
	wstring labelName;
	vector <wstring> aliases;
} mbInfoLabelType;

int getReleases(wstring byWhatType,wstring what,vector <mbInfoReleaseType> &mbTypes);
int getRecordings(wstring byWhatType,wstring what,vector <mbInfoRecordingType> &mbTypes);
int getArtists(wstring byWhatType,wstring what,vector <mbInfoArtistType> &mbTypes);
int getLabels(wstring byWhatType,wstring what,vector <mbInfoLabelType> &mbTypes);
