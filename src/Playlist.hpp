#pragma once

#include <Arduino.h>
#include <FS.h>
#include <ArduinoJson.h>

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

public:
	PlaylistAlloc(TAllocator alloc = TAllocator()) : ARDUINOJSON_NAMESPACE::AllocatorOwner<TAllocator>(alloc), repeatTrack(false), repeatPlaylist(false) { }
	virtual ~PlaylistAlloc() { }

	virtual size_t size() const = 0;

	virtual const String getAbsolutPath(size_t idx) const = 0;

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

// use custom allocators taken from ArduinoJson
template <typename TAllocator>
class WebstreamPlaylistAlloc : public PlaylistAlloc<TAllocator> {
protected:
	char *url;

public:
	explicit WebstreamPlaylistAlloc(const char *_url, TAllocator alloc = TAllocator()) : PlaylistAlloc<TAllocator>(alloc), url(nullptr) {
		url = this->stringCopy(_url);
	}
	virtual ~WebstreamPlaylistAlloc() {
		free(url);
	};

	virtual size_t size() const override { return 1; }
	virtual const String getAbsolutPath(size_t idx) const override { return url; };
	virtual const String getFilename(size_t idx) const override { return url; };

};
using WebstreamPlaylist = WebstreamPlaylistAlloc<DefaultPsramAllocator>;

template <typename TAllocator>
class FolderPlaylistAlloc : public PlaylistAlloc<TAllocator> {
protected:
	char *base;
	char divider;
	char **files;
	size_t capacity;
	size_t count;

public:
	FolderPlaylistAlloc(size_t _capacity, TAllocator alloc = TAllocator()) 
	  : PlaylistAlloc<TAllocator>(alloc), base(nullptr), 
	    files(static_cast<char**>(this->allocate(sizeof(char*) * _capacity))),
		capacity(_capacity), count(0) 
	{
		#if MEM_DEBUG == 1
			assert(files != nullptr);
		#endif
	}

	FolderPlaylist(File &folder) {}

	virtual ~FolderPlaylist() {
		// destory all the evidence!
		for(size_t i=0;i<count;i++) {
			free(files[i]);
		}
		free(files);
		free(base);
	}

	bool setBase(const char *_base) {
		base = this->stringCopy(_base);
		return (base!=nullptr);
	}

	bool push_back(const char *path) {
		if(count >= capacity) {
			return false;
		}

		files[count] = this->stringCopy(path);
		return (files[count]!=nullptr);
	}

	virtual size_t size() const override { return count; };

	virtual const String getAbsolutPath(size_t idx) const override {
		#if MEM_DEBUG == 1
			assert(idx < count);
		#endif
		return String(base) + divider + files[idx];
	};

	virtual const String getFilename(size_t idx) const override {
		#if MEM_DEBUG == 1
			assert(idx < count);
		#endif
		return files[idx];
	};

	virtual void sort(sortFunc func = alphabeticSort) override {
		std::qsort(files, count, sizeof(char*), func);
	}

	virtual void randomize() override {
		if(count <= 1) {
			// we can not randomize less than 2 entries
			return;
		}

		// modern algorithm according to Durstenfeld, R. (1964). Algorithm 235: Random permutation. Commun. ACM, 7, 420.
		for(size_t i=count-1;i>0;i--) {
			size_t j = random(i);
			char *swap = files[i];
			files[i] = files[j];
			files[j] = swap;
		}
	}

protected:
	using PlaylistAlloc<TAllocator>::alphabeticSort;
};
using FolderPlaylist = FolderPlaylistAlloc<DefaultPsramAllocator>;

#if 0
class M3UPlaylist : public Playlist {
public:
	M3UPlaylist(){ }
	virtual ~M3UPlaylist() { }

	virtual size_t size() const {};

	virtual const char* getAbsolutPath(size_t idx) const {};

	virtual const char* getFilename(size_t idx) const {};

	using sortFunc = int(*)(const void*,const void*);
	static int alphabeticSort(const void *x, const void *y) {
		const char *a = static_cast<const char*>(x);
		const char *b = static_cast<const char*>(y);

		return strcmp(a, b);
	}

	virtual void sort(sortFunc func = alphabeticSort) { }

	virtual void randomize() { }
};
#endif

#if 0
class Playlist {
protected:
	bool single;
	size_t capacity;
	union {
		char **files;
		char *file;
	};
	size_t count;

	void init() {
		single = false;
		capacity = 0;
		count = 0;
		files = nullptr;
	}

public:
	explicit Playlist(size_t _len) : single(_len == 1), capacity(_len), files(nullptr), count(0) {
		if(!single) {
			files = static_cast<char**>(malloc(sizeof(char*) * capacity));
		}
	}
    explicit Playlist(const char *text) {
        init();
        single = true;
        file = strdup(text);
    }
	explicit Playlist() {
		init();
	}
	Playlist(Playlist&& rhs) {
		single = rhs.single;
		file = rhs.file;
		capacity = rhs.capacity;
		count = rhs.count;

		rhs.init();
	}

    bool reserve(size_t size) {
        if(capacity || size == 0) {
            // we already have some data, so refuse request
            return false;
        }
        init();
        capacity = size;
        single = (size == 1);
        if(!single) {
            // reserve the array
            files = static_cast<char**>(malloc(sizeof(char*) * capacity));
        }
        return single || (files != nullptr);
    }

    // move operator
    Playlist& operator = (Playlist &&rhs) {
		single = rhs.single;
		file = rhs.file;
		capacity = rhs.capacity;
		count = rhs.count;

		rhs.init();
        return *this;
	}

    //copy operator
    Playlist &operator=(const Playlist &rhs) {
		single = rhs.single;
		file = rhs.file;
		capacity = rhs.capacity;
		count = rhs.count;
        return *this;
    }

    // access to element like a POD array
	char* operator[](size_t idx) const {
        if(single)
            return file;
		return files[idx];
	}

    char* &operator[](size_t idx) {
        if(single)
            return file;
		return files[idx];
    }

    char **begin() {
        return files;
    }

    size_t size() const { return count; }

    size_t elemSize() const {return sizeof(file); }

	Playlist& operator = (const char *text) {
		if(single) {
			file = strdup(text);
		} else {
			push_back(text);
		}
		return *this;
	}

	bool push_back(const char* path) {
		if(count >= capacity) {
			// the memory is full, go away
			return false;
		}
		char **slot = (single) ? &file : files + count;
		*slot = strdup(path);
		count++;
        return true;
	}

	void invalidate() {
		if(!single) {
			// free every element of the array...
			for(size_t i=0;i<count;i++){
				free(files[i]);
			}
		}
		// ...and the array itself
		free(files);	// this is a union, so we only need to free one pointer (either a char** or a char*)
		count = 0;
		capacity = 0;
	}
};
#endif
