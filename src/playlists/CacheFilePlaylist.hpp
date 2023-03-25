#pragma once

#include <WString.h>
#include <stdint.h>
#include <optional>
#include <rom/crc.h>

#include "../Playlist.h"
#include "FolderPlaylist.hpp"

class CacheFilePlaylist : public FolderPlaylist {
public:
    CacheFilePlaylist(char divider = '/') : FolderPlaylist(divider), headerFlags(Flags()), headerValid(false) { }
	CacheFilePlaylist(File &cacheFile, char divider = '/') : FolderPlaylist(divider), headerFlags(Flags()), headerValid(false) {
        deserialize(cacheFile);
	}
    virtual ~CacheFilePlaylist() {
        this->destroy();
    }

    static bool serialize(File &target, const FolderPlaylist &list) {
        // write the header into the file
        // header is big endian 
        BinaryCacheHeader header;
        size_t ret;

        // first is the magic marker
        header.magic = magic;
        ret = write(target, magic);

        // the header version & flags
        header.version = version;
        ret += write(target, version);

        // update flags
        Flags flags;
        flags.relative = list.isRelative();

        header.flags = flags;
        ret += write(target, flags);

        // write the number of entries and the crc (not implemented yet)
        header.count = list.size();
        ret += write(target, header.count);
        header.crc = crcBase;
        header.sep = separator;
        ret += write(target, calcCRC(header));

        ret += target.write(header.sep);

        if(ret != headerSize) {
            #ifdef MEM_DEBUG
                assert(ret != headerSize);
            #endif
            return false;
        }

        return writeEntries(target, list);
    }

    static std::optional<const String> getCachefilePath(File &dir) {
        if(!dir.isDirectory())
            return {};

        const char *path = getPath(dir);
        return String(path) + "/" + playlistCacheFile;
    }

    bool deserialize(File &cache) {
        
        if(!checkHeader(cache)) {
            return false;
        }

        // destroy old data, if present
        if(isValid()) {
            this->clear();
        }

        deserializeHeader(cache);

        // reserve the memory
        if(!this->reserve(headerCount)) {
            // we failed to reserve the needed memory
            return false;
        }
 
        // everything was ok read the files
        return readEntries(cache);
    }

    bool deserializeOldPlaylist(File &cache) {
        if(!isOldPlaylist(cache)){
            return false;
        }

        // iterate through the file to find the count
        size_t entries = 0;;
        while(cache.available()) {
            const char c = cache.read();
            if(c == '#') {
                entries++;
            }
        }
        cache.seek(0);
        log_n("entries: %d", entries);

        // destroy data
        this->clear();

        if(!this->reserve(entries)){
            // we failed to get the memory
            return false;
        }
        
        log_n("flags: %04x", headerFlags);
        return readEntries(cache);
    }

    static bool isOldPlaylist(File &cache) {
        uint8_t prop = 0;

        // try to guess, if we have an old playlist
        int result = checkHeader(cache);
        if(result == -1) {
            // we failed at the magic & version part
            prop++;
        } else if(result > 0 || result == -2) {
            // we have a perfect header or a crc fail
            return false;
        }

        // count the lines in the file (there should be at least 1 '#')
        size_t countSince = 0;
        while(cache.available()) {
            const char c = cache.read();
            if(c == '#' && countSince > 4) {
                // we only accep separators, which are at least 4 characters apart (with 4 chars the filename could only be ".xyz")
                prop++;
                countSince = 0;
            } else {
                countSince++;
            }
        }

        cache.seek(0);  // rewinde file before leaving
        return (prop > 2);  // we found no header and at least 1 speparator
    }

    bool setBase(const char *_base) {
		this->base = this->stringCopy(_base);
        headerFlags.relative = (this->base);
		return (this->base);
	}

    bool setBase(const String base) {
		return setBase(base.c_str());
	}

    virtual bool isValid() const override {
        return headerValid && FolderPlaylist::isValid();
    }

    void clear() {
		FolderPlaylist::destroy();
		FolderPlaylist::init();
        headerValid = false;
        headerFlags = Flags();
        headerCount = 0;
	}

protected:
    // bitwise flags for future use
    struct Flags {
        bool relative;

        Flags(): relative(false) {}

        operator uint16_t() const {
            // this function casts the flags into a 16bit wide bit array
            // f.e. 
            //      uint16_t flags = 0x00;
            //      flags |= (flag << 0);
            //      flags |= (otherFlag << 1);
            //      return flags;
            uint16_t bitfield = 0x00;

            bitfield |= (relative << 0);
            return bitfield;
        }

        Flags &operator=(const uint16_t binary) {
            // here we get a bitfield and break it down into our internal variables
            // f.e.
            //      flag = binary & _BV(0);
            //      otherFlag = binary & _BV(1);

            relative = binary & _BV(0);
            return *this;
        }
    };

    struct BinaryCacheHeader
    {
        uint16_t magic;
        uint16_t version;
        uint16_t flags;
        uint32_t count;
        uint32_t crc;
        uint8_t sep;
    };
    static constexpr uint16_t magic = 0x4346;   //< Magic header "CF"
    static constexpr uint16_t version = 1;      //< Current cache file version, if header or enconding changes, this has to be incremented
    static constexpr uint32_t crcBase = 0x00;   //< starting value of the crc calculation
    static constexpr size_t headerSize = 15;    //< the expected size of the header: magic(2) + version(2) + flags(2) + count(4) + crc(4) + separator(1)

    Flags headerFlags;                          //< A 16bit bitfield of flags
    bool headerValid;
    size_t headerCount;

    static constexpr char separator = '#';      //< separator for all entries

    static int checkHeader(File &cache) {
        int ret = 1;
        // read the header from the file
        BinaryCacheHeader header;

        header.magic = read16(cache);
        header.version = read16(cache);

        // first checkpoint
        if(header.magic != magic || header.version != version) {
            // header did not match, bail out
            ret = -1;
        }

        // read the flags and the count
        header.flags = read16(cache);
        header.count = read32(cache);

        // second checkpoint, crc and separator
        header.crc = crcBase;
        uint32_t crc = read32(cache);
        header.sep = cache.read();
        if((ret > 0) && (calcCRC(header) != crc || header.sep != separator)) {
            // crc missmatch, bail out
            ret = -2;
        }

        cache.seek(0);
        return ret;
    }

    bool deserializeHeader(File &cache) {
        headerValid = false;
        if(checkHeader(cache) < 1) {
            return false;
        }
        // header is valid, read on
        headerValid = true;

        // ignore magic & version
        cache.seek(sizeof(BinaryCacheHeader::magic) + sizeof(BinaryCacheHeader::version));

        // read the flags and the count
        headerFlags = read16(cache);
        headerCount = read32(cache);

        cache.seek(sizeof(BinaryCacheHeader::crc) + sizeof(BinaryCacheHeader::sep), SeekMode::SeekCur);
        return true;
    }

    // helper function to write 16 bit in big endian
    static size_t write(File &f, uint16_t value) {
        size_t ret;

        ret = f.write(value >> 8);
        ret += f.write(value);
        return ret;
    }

    // helper function to write 32 bit in big endian
    static size_t write(File &f, uint32_t value) {
        size_t ret;

        ret = write(f, uint16_t(value >> 16));
        ret += write(f, uint16_t(value));
        return ret;
    }

    // helper fuction to read 16 bit in big endian
    static uint16_t read16(File &f) {
        return (f.read() << 8) | f.read();
    }

    // helper fuction to read 32 bit in big endian
    static uint32_t read32(File &f) {
        return (read16(f) << 16) | read16(f);
    }

    static uint32_t calcCRC(const BinaryCacheHeader &header) {
        // add all header fields individually since BinaryCacheHeader is not packed
        uint32_t ret = crc32_le(0, reinterpret_cast<const uint8_t*>(&header.magic), sizeof(header.magic));
        ret = crc32_le(ret, reinterpret_cast<const uint8_t*>(&header.version), sizeof(header.version));
        ret = crc32_le(ret, reinterpret_cast<const uint8_t*>(&header.flags), sizeof(header.flags));
        ret = crc32_le(ret, reinterpret_cast<const uint8_t*>(&header.count), sizeof(header.count));
        ret = crc32_le(ret, reinterpret_cast<const uint8_t*>(&header.crc), sizeof(header.crc));
        ret = crc32_le(ret, reinterpret_cast<const uint8_t*>(&header.sep), sizeof(header.sep));
        return ret;
    }

    bool writeEntries(File &f) const {
        return writeEntries(f, *this);
    }

    static bool writeEntries(File &f, const FolderPlaylist &list) {
        // if flag is set, use relative path
        if(list.isRelative()) {
            f.write(reinterpret_cast<const uint8_t*>(list.getBase()), strlen(list.getBase()));
            f.write(separator);
        }

        // write all entries with the separator to the file
        for(size_t i=0;i<list.size();i++) {
            const String path = list.getAbsolutePath(i);
            if(f.write(reinterpret_cast<const uint8_t*>(path.c_str()), path.length()) != path.length()) {
                return false;
            }
            f.write(separator);
        }
        return true;
    }

    bool readEntries(File &f) {
        // if flag is set, use relative path
        if(headerFlags.relative) {
            const String basePath = f.readStringUntil(separator);
            this->setBase(basePath);
        }

        while(f.available()) {
            const String path = f.readStringUntil(separator);
            if(!this->push_back(path)){
                return false;
            }
        }

        return true;
    }
};
using CacheFilePlaylist = CacheFilePlaylistAlloc<DefaultPsramAllocator>;
