#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "Common.h"

#if MEM_DEBUG == 1
	#warning Memory access guards are enabled. Disable MEM_DEBUG for production builds
#endif

using sortFunc = int(*)(const void*,const void*);

class Playlist {
protected:

	virtual void destroy() { }

	bool repeatTrack;
	bool repeatPlaylist;

public:
	Playlist() : repeatTrack(false), repeatPlaylist(false) { }
	virtual ~Playlist() { }

	bool getRepeatTrack() const { return repeatTrack; }
	void setRepeatTrack(bool newVal) { repeatTrack = newVal; }

	bool getRepeatPlaylist() const { return repeatPlaylist; }
	void setRepeatPlaylist(bool newVal) { repeatPlaylist = newVal; }

	virtual size_t size() const = 0;

	virtual bool isValid() const = 0;

	virtual const String getAbsolutePath(size_t idx) const = 0;

	virtual const String getFilename(size_t idx) const = 0;

	static int alphabeticSort(const void *x, const void *y) {
		const char *a = static_cast<const char*>(x);
		const char *b = static_cast<const char*>(y);

		return strcmp(a, b);
	}

	virtual void sort(sortFunc func = alphabeticSort) { }

	virtual void randomize() { }
};
