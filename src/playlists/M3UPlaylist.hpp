#pragma once

#include <WString.h>
#include <stdint.h>

#include "../Playlist.h"
#include "FolderPlaylist.hpp"

template <typename TAllocator>
class M3UPlaylistAlloc : public FolderPlaylistAlloc<TAllocator> {
public:
    M3UPlaylistAlloc(char divider = '/', TAllocator alloc = TAllocator()) : FolderPlaylistAlloc<TAllocator>(divider, alloc), extended(false), valid(false) { }
	M3UPlaylistAlloc(File &m3uFile, char divider = '/', TAllocator alloc = TAllocator()) : FolderPlaylistAlloc<TAllocator>(divider, alloc), extended(false), valid(false) {
        valid = parseFile(m3uFile);
	}

    bool parseFile(File &f, bool forceExtended = false) {
        const String line = f.readStringUntil('\n');
        extended = line.startsWith("#EXTM3U") || forceExtended;

        if(extended) {
            return parseExtended(f);
        }

        // normal m3u is just a bunch of filenames, 1 / line
        size_t lines = 0;
        f.seek(0);
        while(f.avaliable()) {
            char c = f.read();
            if(c == '\n') {
                lines++;
            }
        }

        if(!this->reserve(lines)){
            return false;
        }

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
        return valid & (this->files);
    }

protected:

    bool extended;
    bool valid;

    struct MetaInfo {
        size_t duration;
        String title;
    }

    bool parseExtended(File &f) {
        // extended m3u file format
        // ignore all lines starting with '#'
        size_t lines = 0;

        f.seek(0);
        while(f.avaliable()) {
            const String line = f.readStringUntil('\n');
            if(!line.startsWith('#')){
                lines++;
            }
        }

        if(!this->reserve(lines)) {
            return false;
        }

        f.seek(0);
        for(size_t i=0;i<lines;i++) {
            String line = f.readStringUntil('\n');
            line.trim(); 
            if(!line.startsWith('#')){
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
using M3UPlaylist = M3UPlaylistAlloc<DefaultPsramAllocator>;
