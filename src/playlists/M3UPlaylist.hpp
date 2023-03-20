#pragma once

#include <WString.h>
#include <stdint.h>
#include <rom/crc.h>

#include "../Playlist.h"
#include "FolderPlaylist.hpp"

template <typename TAllocator>
class M3UPlaylistAlloc : public FolderPlaylistAlloc<TAllocator> {
protected:

    uint8_t version;
    bool valid;

public:
    M3UPlaylistAlloc(char divider = '/', TAllocator alloc = TAllocator()) : FolderPlaylistAlloc<TAllocator>(divider, alloc), version(1), valid(false) { }
	M3UPlaylistAlloc(File &m3uFile, char divider = '/', TAllocator alloc = TAllocator()) : FolderPlaylistAlloc<TAllocator>(divider, alloc), version(1), valid(false) {
        parseFile(m3uFile);
	}

    bool parseFile(File &f) {

    }

    virtual bool isValid() const override {
        return valid & (this->files);
    }
};
using M3UPlaylist = M3UPlaylistAlloc<DefaultPsramAllocator>;
