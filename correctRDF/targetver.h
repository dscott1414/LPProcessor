/*
	targetver.h - Windows SDK version pin for correctRDF

	Overview:
		Includes SDKDDKVer.h so the build targets the newest installed
		Windows platform. No logic of its own.

	Pipeline position:
		Included only via stdafx.h.
*/
#pragma once

// Including SDKDDKVer.h defines the highest available Windows platform.

// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.

#include <SDKDDKVer.h>
