#pragma once

#include <stdint.h>
#include <WString.h>

#ifndef PIO_UNIT_TESTING
	#include <FS.h>
#endif

#include "../Playlist.h"

template <typename TAllocator>
class FolderPlaylistAlloc : public PlaylistAlloc<TAllocator> {
protected:
	char *base;
	char divider;
	char **files;
	size_t capacity;
	size_t count;

public:
	FolderPlaylistAlloc(size_t _capacity, char _divider = '/', TAllocator alloc = TAllocator()) 
	  : PlaylistAlloc<TAllocator>(alloc), base(nullptr), divider(_divider),
	    files(static_cast<char**>(this->allocate(sizeof(char*) * _capacity))),
		capacity(_capacity), count(0) 
	{
		#if MEM_DEBUG == 1
			assert(files != nullptr);
		#endif
	}
	FolderPlaylistAlloc(char _divider = '/', TAllocator alloc = TAllocator()) 
	  : PlaylistAlloc<TAllocator>(alloc), base(nullptr),  divider(_divider),
	    files(nullptr), capacity(0), count(0) 
	{ }

	FolderPlaylistAlloc(File &folder, char _divider = '/', TAllocator alloc = TAllocator()) : PlaylistAlloc<TAllocator>(alloc), divider(_divider) {
		createFromFolder(folder);
	}

	virtual ~FolderPlaylistAlloc() {
		destory();
	}

	bool createFromFolder(File &folder) {
		// This is not a folder, so bail out
		if(!folder || !folder.isDirectory()){
			return false;
		}

		// clean up any previously used memory
		clear();

		// since we are enumerating, we don't have to think about absolute files with different bases 
		const char *path;
		#if ESP_ARDUINO_VERSION_MAJO >= 2
			path = folder.path();
		#else
			path = folder.name();
		#endif
		base = this->stringCopy(path);

		// enumerate all files in the folder, we have to do it twice
		constexpr size_t allocUnit = 255;	// we start with 255 entries and double them, if needed
		size_t allocCount = 1;
		reserve(allocUnit * allocCount);
		while(true) {
			File entry = folder.openNextFile();
			if(!entry) {
				break;
			}
			if(entry.isDirectory()) {
				continue;
			}
			const char *path;
			#if ESP_ARDUINO_VERSION_MAJO >= 2
				path = entry.name();
			#else
				path = entry.name();
				// remove base, since in Arduino 1, name return the path
				path = path + strlen(base) + 1;
			#endif
			if(fileValid(path)) {
				// push this file into the array
				bool success = push_back(path);
				if(!success) {
					// we need more memory D:
					allocCount++;
					if(!reserve(allocUnit * allocCount)) {
						return false;
					}
					// we should have more memory now
					push_back(path);
				}
			}
		}
		// resize memory to fit our count
		reserve(count);

		return true;
	}

	bool reserve(size_t _cap) {
		if(_cap < count) {
			// we do not support reducing below the current size
			return false;
		}

		char **tmp = static_cast<char**>(this->reallocate((capacity > 0 ) ? files : nullptr, sizeof(char*) * _cap));
		if(!tmp) {
			// we failed to get the needed memory D:
			return false;
		}
		files = tmp;
		capacity = _cap;
		return true;
	}

	bool setBase(const char *_base) {
		base = this->stringCopy(_base);
		return (base!=nullptr);
	}

	bool push_back(const char *path) {
		if(count >= capacity) {
			return false;
		}
		if(!fileValid(path)) {
			return false;
		}

		// here we check if we have to cut up the path (currently it's only a crude check for absolute paths)
		if(base && path[0] == '/') {
			// we are in relative mode and got an absolute path, check if the path begins with our base
			// Also check if the path is so short, that there is no space for a filename in it
			if((strncmp(path, base, strlen(base)) != 0) || (strlen(path) < strlen(base) + strlen("/.abc"))) {
				// we refuse files other than our base
				return false;
			}
			path = path + strlen(base) + 1;	// modify pointer to the end of the path
		}

		char *tmp = this->stringCopy(path);
		if(!tmp) {
			// stringCopy failed
			return false;
		}
		files[count++] = tmp;
		return true;
	}

	bool push_back(const String path) {
		return push_back(path.c_str());
	}

	void clear() {
		destory();
		init();
	}

	void setDivider(char _divider) { divider = _divider; }
	bool getDivider() const { return divider; }

	virtual size_t size() const override { return count; };

	virtual bool isValid() const override { return (files); }

	virtual const String getAbsolutePath(size_t idx) const override {
		#if MEM_DEBUG == 1
			assert(idx < count);
		#endif
		if(base) {
			// we are in relative mode
			return String(base) + divider + files[idx];
		}
		return files[idx];
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
		if(count < 2) {
			// we can not randomize less than 2 entries
			return;
		}

		// Knuth-Fisher-Yates-algorithm to randomize playlist
		// modern algorithm according to Durstenfeld, R. (1964). Algorithm 235: Random permutation. Commun. ACM, 7, 420.
		for(size_t i=count-1;i>0;i--) {
			size_t j = random(i);
			char *swap = files[i];
			files[i] = files[j];
			files[j] = swap;
		}
	}

protected:
	void destory() {
		// destory all the evidence!
		for(size_t i=0;i<count;i++) {
			this->deallocate(files[i]);
		}
		this->deallocate(files);
		this->deallocate(base);
	}

	void init() {
		base = nullptr;
		files = nullptr;
		capacity = 0;
		count = 0;
		divider = '/';
	}

	// Check if file-type is correct
	bool fileValid(const char *_fileItem) {
		if(!_fileItem)
			return false;

		// if we are in absolute mode...
		if(!base) {
			_fileItem = strrchr(_fileItem, divider);	//... extract the file name from path
		}

		return (!startsWith(_fileItem, (char *) "/.")) && (
				endsWith(_fileItem, ".mp3") || endsWith(_fileItem, ".MP3") ||
				endsWith(_fileItem, ".aac") || endsWith(_fileItem, ".AAC") ||
				endsWith(_fileItem, ".m3u") || endsWith(_fileItem, ".M3U") ||
				endsWith(_fileItem, ".m4a") || endsWith(_fileItem, ".M4A") ||
				endsWith(_fileItem, ".wav") || endsWith(_fileItem, ".WAV") ||
				endsWith(_fileItem, ".flac") || endsWith(_fileItem, ".FLAC") ||
				endsWith(_fileItem, ".asx") || endsWith(_fileItem, ".ASX"));
	}

	using PlaylistAlloc<TAllocator>::alphabeticSort;
};
using FolderPlaylist = FolderPlaylistAlloc<DefaultPsramAllocator>;