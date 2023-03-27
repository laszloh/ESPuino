#pragma once

#include <WString.h>
#include <stdint.h>

#include "../Playlist.h"
#include "FolderPlaylist.hpp"

class M3UPlaylist : public FolderPlaylist {
public:
    M3UPlaylist(size_t cap = 64, char divider = '/') : FolderPlaylist(cap, divider), extended(false), valid(false) { }
	M3UPlaylist(File &m3uFile, size_t cap = 64,char divider = '/') : FolderPlaylist(cap, divider), extended(false), valid(false) {
        valid = parseFile(m3uFile);
	}
    ~M3UPlaylist() { }

    bool parseFile(File &f, bool forceExtended = false) {
        const String line = f.readStringUntil('\n');
        extended = line.startsWith("#EXTM3U") || forceExtended;

        if(extended) {
            return parseExtended(f);
        }

        // normal m3u is just a bunch of filenames, 1 / line
        f.seek(0);
        while(f.available()) {
            String line = f.readStringUntil('\n');
            line.trim();
            if(!push_back(line)) {
                return false;
            }
        }
		// resize memory to fit our count
		files.shrink_to_fit();

        valid = true;
        return true;
    }

    virtual bool isValid() const override {
        return valid && FolderPlaylist::isValid();
    }

protected:

    bool extended;
    bool valid;

    bool parseExtended(File &f) {
        // extended m3u file format
        // ignore all lines starting with '#'

        while(f.available()) {
            String line = f.readStringUntil('\n');
            if(!line.startsWith("#")){
                // this something we have to save
                line.trim(); 
                if(!this->push_back(line)) {
                    return false;
                }
            }
        }
        // resize memory to fit our count
		files.shrink_to_fit();

        valid = true;
        return true;
    }

};
