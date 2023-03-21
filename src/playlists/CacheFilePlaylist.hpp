#pragma once

#include <WString.h>
#include <stdint.h>
#include <rom/crc.h>

#include "../Playlist.h"
#include "FolderPlaylist.hpp"

template <typename TAllocator>
class CacheFilePlaylistAlloc : public FolderPlaylistAlloc<TAllocator> {
public:
    CacheFilePlaylistAlloc(char divider = '/', TAllocator alloc = TAllocator()) : FolderPlaylistAlloc<TAllocator>(divider, alloc), flags(Flags()), headerValid(false) { }
	CacheFilePlaylistAlloc(File &cacheFile, char divider = '/', TAllocator alloc = TAllocator()) : FolderPlaylistAlloc<TAllocator>(divider, alloc), flags(Flags()), headerValid(false) {
        deserialize(cacheFile);
	}

    bool serialize(File &target) const {
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
        header.flags = flags;
        ret += write(target, flags);

        // write the number of entries and the crc (not implemented yet)
        header.count = this->count;
        ret += write(target, header.count);
        header.crc = crcBase;
        ret += write(target, calcCRC(header));

        ret += target.write(separator);

        if(ret != headerSize) {
            #ifdef MEM_DEBUG
                assert(ret != headerSize);
            #endif
            return false;
        }

        return writeEntries(target);
    }

    bool deserialize(File &cache) {
        // read the header from the file
        BinaryCacheHeader header;

        header.magic = read16(cache);
        header.version = read16(cache);

        // first checkpoint
        if(header.magic != magic || header.version != version) {
            // header did not match, bail out
            return false;
        }

        // read the flags and the count
        header.flags = read16(cache);
        header.count = read32(cache);

        // second checkpoint, crc and separator
        header.crc = read16(cache);
        if(calcCRC(header) != 0x00 || cache.read() != separator) {
            // here we use the feature of the crc16_le that the crc over a block with a correct crc is zero
            return false;
        }

        // destroy old data, if present
        if(isValid()) {
            destroy();
        }

        // header was ok
        headerValid = true;

        // reserve the memory
        bool success = this->reserve(header.count);
        if(!success) {
            // we failed to reserve the needed memory
            return false;
        }
 
        // everything was ok read the files
        return readEntries(cache);
    }

    virtual bool isValid() const override {
        return headerValid & (this->files);
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

    struct __attribute__((packed, aligned(1))) BinaryCacheHeader
    {
        uint16_t magic;
        uint16_t version;
        uint16_t flags;
        uint32_t count;
        uint16_t crc;
    };
    static constexpr uint16_t magic = 0x4346;   //< Magic header "CF"
    static constexpr uint16_t version = 1;      //< Current cache file version, if header or enconding changes, this has to be incremented
    static constexpr uint16_t crcBase = 0x00;   //< starting value of the crc calculation
    static constexpr size_t headerSize = 13;    //< the expected size of the header: magic(2) + version(2) + flags(2) + count(4) + crc(2) + separator(1)

    Flags flags;                                //< A 16bit bitfield of flags
    bool headerValid;

    static constexpr char separator = '#';      //< separator for all entries

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

    uint16_t calcCRC(const BinaryCacheHeader &header) {
        // add all header fields individually since BinaryCacheHeader is not packed
        return crc16_le(0x0000, &header, sizeof(BinaryCacheHeader));
    }

    bool writeEntries(File &f) const {
        // write all entries with the separator to the file
        for(size_t i=0;i<this->count;i++) {
            const String path = this->getAbsolutePath(i);
            if(f.write(static_cast<const uint8_t*>(path.c_str()), path.len()) != path.len()) {
                return false;
            }
            f.write(separator);
        }
        return true;
    }

    bool readEntries(File &f) {
        // if flag is set, use relative path
        if(flags.relative) {
            const String basePath = f.readStringUntil(separator);
            this->setBase(basePath);
        }
        // test if we need this
        f.seek(1, SeekCur);

        for(size_t i=0;i<this->capacity;i++) {
            const String path = f.readStringUntil(separator);
            // test if we need this
            f.seek(1, SeekCur);
            if(!this->push_back(path)){
                return false;
            }
        }

        return true;
    }
};
using CacheFilePlaylist = CacheFilePlaylistAlloc<DefaultPsramAllocator>;
