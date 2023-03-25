#pragma once

#include <stdint.h>
#include <WString.h>
#include <FS.h>
#include <string>
#include <vector>
#include <random>

#include "../Playlist.h"

class FolderPlaylist : public Playlist {
protected:
	pstring base;
	std::vector<pstring, PsramAllocator<pstring>> files;
	char divider;

public:
	FolderPlaylist(size_t _capacity, char _divider = '/') 
	  : base(pstring()), divider(_divider), files(std::vector<pstring, PsramAllocator<pstring>>(_capacity))
	{ }
	FolderPlaylist(char _divider = '/') 
	  : base(pstring()),  divider(_divider), files(std::vector<pstring, PsramAllocator<pstring>>())
	{ }

	FolderPlaylist(File &folder, char _divider = '/') : divider(_divider) {
		createFromFolder(folder);
	}

	virtual ~FolderPlaylist() {
		destroy();
	}

	bool createFromFolder(File &folder) {
		// This is not a folder, so bail out
		if(!folder || !folder.isDirectory()){
			return false;
		}

		// clean up any previously used memory
		clear();

		// since we are enumerating, we don't have to think about absolute files with different bases 
		base = getPath(folder);

		// enumerate all files in the folder, we have to do it twice
		
		
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
				path = path + base.length() + 1;
			#endif
			if(fileValid(path)) {
				// push this file into the array
				bool success = push_back(path);
				if(!success) {
					return false;
				}
			}
		}
		// resize memory to fit our count
		files.shrink_to_fit();

		return true;
	}

	void setBase(const char *_base) {
		base = _base;
	}

	void setBase(const String _base) {
		base = _base.c_str();
	}

	const char *getBase() const {
		return base.c_str();
	}

	bool isRelative() const {
		return base.length();
	}

	bool push_back(const char *path) {
		if(!fileValid(path)) {
			return false;
		}

		// here we check if we have to cut up the path (currently it's only a crude check for absolute paths)
		if(isRelative() && path[0] == '/') {
			// we are in relative mode and got an absolute path, check if the path begins with our base
			// Also check if the path is so short, that there is no space for a filename in it
			const size_t pathLen = strlen(path) - (base.length() + strlen("/.abc"));
			if( (strncmp(path, base.c_str(), base.length()) != 0) || (strlen(path) <  (base.length() + strlen("/.abc")))) {
				// we refuse files other than our base
				return false;
			}
			path = path + base.length() + 1;	// modify pointer to the end of the path
		}

		files.push_back(path);
		return true;
	}

	bool push_back(const String path) {
		return push_back(path.c_str());
	}

	void clear() {
		destroy();
		init();
	}

	void setDivider(char _divider) { divider = _divider; }
	bool getDivider() const { return divider; }

	virtual size_t size() const override { return files.size(); };

	virtual bool isValid() const override { return files.size(); }

	virtual const String getAbsolutePath(size_t idx) const override {
		#if MEM_DEBUG == 1
			assert(idx < files.size());
		#endif
		if(isRelative()) {
			// we are in relative mode
			return String(base.c_str()) + divider + files[idx].c_str();
		}
		return String(files[idx].c_str());
	};

	virtual const String getFilename(size_t idx) const override {
		#if MEM_DEBUG == 1
			assert(idx < files.size());
		#endif
		if(isRelative()) {
			return String(files[idx].c_str());
		}
		pstring path = files[idx];
		return String(path.substr(path.find_last_of("/") + 1).c_str());
	};

	virtual void sort(sortFunc func = alphabeticSort) override {
		std::sort(files.begin(), files.end());
	}

	virtual void randomize() override {
		if(files.size() < 2) {
			// we can not randomize less than 2 entries
			return;
		}

		// randomize using the "normal" random engine and shuffle
		std::default_random_engine rnd(millis());
		std::shuffle(files.begin(), files.end(), rnd);
	}

protected:
	virtual void destroy() override {
		files.clear();
		base = pstring();
	}

	void init() {
		divider = '/';
	}

	// Check if file-type is correct
	bool fileValid(const char *_fileItem) {
		if(!_fileItem)
			return false;

		// if we are in absolute mode...
		if(!isRelative()) {
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

};
