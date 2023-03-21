#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "Common.h"

#if MEM_DEBUG == 1
	#warning Memory access guards are enabled. Disable MEM_DEBUG for production builds
#endif

using sortFunc = int(*)(const void*,const void*);

struct DefaultPsramAllocator {
  void* allocate(size_t size) {
	if(psramInit()) {
		return ps_malloc(size);
	} else {
    	return malloc(size);
	}
  }

  void deallocate(void* ptr) {
    free(ptr);
  }

  void* reallocate(void* ptr, size_t new_size) {
	if(psramInit()) {
		return ps_realloc(ptr, new_size);
	} else {
    	return realloc(ptr, new_size);
	}
  }
};

template <typename TAllocator>
class PlaylistAlloc : protected ARDUINOJSON_NAMESPACE::AllocatorOwner<TAllocator> {
protected:
 	char *stringCopy(const char *string) {
		size_t len = strlen(string);
		char *ret = static_cast<char*>(this->allocate(len + 1));
		if(ret) {
			memcpy(ret, string, len + 1);		// this is a zero-terminated string, so also copy the zero
		}
		return ret;
	}

	virtual void destroy() { }

	bool repeatTrack;
	bool repeatPlaylist;

public:
	PlaylistAlloc(TAllocator alloc = TAllocator()) : ARDUINOJSON_NAMESPACE::AllocatorOwner<TAllocator>(alloc), repeatTrack(false), repeatPlaylist(false) { }
	virtual ~PlaylistAlloc() { }

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
using Playlist = PlaylistAlloc<DefaultPsramAllocator>;
