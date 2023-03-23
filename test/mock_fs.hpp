#pragma once

#include <list>
#include <vector>

#include <WString.h>
#include <FS.h>
#include <FSImpl.h>

namespace mockfs
{

struct Node {
    ~Node() {
        files.clear();
        content.clear();
    }
    String fullPath{""};
    bool valid{false};
    bool isDir{false};
    std::vector<uint8_t> content{std::vector<uint8_t>()};
    std::vector<Node> files{std::vector<Node>()};
};

class MockFileImp : public fs::FileImpl {
protected:
    Node node;
    std::vector<uint8_t>::iterator cit;
    bool readOnly;
    bool fileOpen;
    time_t lastWrite;
    std::vector<Node>::iterator it;

public:
    static fs::FileImplPtr open(Node n, bool ro) {
        return std::shared_ptr<fs::FileImpl>(new MockFileImp(n, ro));
    }

    MockFileImp(Node n, bool ro) : node(n), cit(node.content.begin()), readOnly(ro), fileOpen(true), lastWrite(0), it(node.files.begin()) { }

    virtual ~MockFileImp() {
        close();
    }

    virtual size_t write(const uint8_t *buf, size_t size) override {
        if(readOnly)
            return 0;

        lastWrite = time(NULL);
        std::copy(buf, buf + size, cit);
        cit += size;
        return size;
    }

    virtual size_t read(uint8_t* buf, size_t size) override {
        // basic check if we reach the end of the file
        const size_t cap = std::distance(cit, node.content.end());
        const size_t rsize = std::min(size, cap);

        std::copy(cit, cit + rsize, buf);
        cit += rsize;
        return rsize;
    }

    virtual void flush() override { }

    virtual bool seek(uint32_t pos, SeekMode mode) override {
        switch(mode) {
            case SeekCur:
                // test if we go over the end
                if((cit + pos) >= node.content.end()) {
                    return false;
                }
                cit += pos;
                break;
            
            case SeekSet:
                if(pos > node.content.size()) {
                    return false;
                }
                cit = node.content.begin() + pos;
                break;

            case SeekEnd:
                if(pos > node.content.size()) {
                    return false;
                }
                cit = node.content.end() - pos;
                break;
        }
        return true;
    }

    virtual size_t position() const override {
        return std::distance<std::vector<uint8_t>::const_iterator>(node.content.begin(), cit);
    }

    virtual size_t size() const override { return node.content.size(); };

    virtual bool setBufferSize(size_t size) { return false; };
    
    virtual void close() override { 
        cit = node.content.begin();
        fileOpen = false;
        node.content.shrink_to_fit();
    }

    virtual time_t getLastWrite() override { return lastWrite; };

    virtual const char* name() const override { return node.fullPath.c_str(); };

    virtual boolean isDirectory(void) override { return node.isDir; };

    virtual fs::FileImplPtr openNextFile(const char* mode) override {
        if(!node.isDir || it >= node.files.end())
            return nullptr;
        auto newFilePtr = std::shared_ptr<fs::FileImpl>(new MockFileImp(*it, strcmp(mode, "R") == 0));
        it++;
        return newFilePtr;
    };

    virtual boolean seekDir(long position) {
        uint32_t offset = static_cast<uint32_t>(position);
        if(!node.isDir || (it + offset) >= node.files.end())
            return false;
        it += offset;
        return true;
    }

    virtual String getNextFileName(void) {
        auto next = it++;
        return next->fullPath;
    }

    virtual void rewindDirectory(void) override {
        it = node.files.begin();
    }

    virtual operator bool() override {
        return fileOpen;
    }
};

} // namespace mockfs
