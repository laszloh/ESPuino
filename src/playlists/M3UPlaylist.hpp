#pragma once

#include <WString.h>
#include <stdint.h>

#include "../Playlist.h"
#include "FolderPlaylist.hpp"

class M3UPlaylist : public FolderPlaylist {
public:
    M3UPlaylist(char divider = '/') : FolderPlaylist(divider), extended(false), valid(false) { }
	M3UPlaylist(File &m3uFile, char divider = '/') : FolderPlaylist(divider), extended(false), valid(false) {
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
        size_t lines = 0;
        f.seek(0);
        while(f.available()) {
            char c = f.read();
            if(c == '\n') {
                lines++;
            }
        }

        // if(!this->reserve(lines)){
        //     return false;
        // }

        f.seek(0);
        for(size_t i=0;i<lines;i++) {
            String line = f.readStringUntil('\n');
            line.trim();
            if(!this->push_back(line)) {
                return false;
            }
        }

        valid = true;
        return true;
    }

    virtual bool isValid() const override {
        // return valid && (this->files);
    }

protected:

    bool extended;
    bool valid;

    struct MetaInfo {
        size_t duration;
        String title;
    };

    bool parseExtended(File &f) {
        // extended m3u file format
        // ignore all lines starting with '#'
        size_t lines = 0;

        f.seek(0);
        while(f.available()) {
            const String line = f.readStringUntil('\n');
            if(!line.startsWith("#")){
                lines++;
            }
        }

        // if(!this->reserve(lines)) {
        //     return false;
        // }

        f.seek(0);
        for(size_t i=0;i<lines;i++) {
            String line = f.readStringUntil('\n');
            line.trim(); 
            if(!line.startsWith("#")){
                // this something we have to save
                if(!this->push_back(line)) {
                    return false;
                }
            }
        }

        valid = true;
        return true;
    }

};
