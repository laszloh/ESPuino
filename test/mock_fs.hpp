#pragma once

#include <list>
#include <vector>

namespace mockfs
{

struct Node {
    ~Node() { content.clear(); }
    String fullPath{""};
    bool valid{false};
    bool isDir{false};
    std::vector<uint8_t> content;
    std::list<Node> files{std::list<Node>()};
};

class MockFileImp : public fs::FileImpl {
protected:
    Node node;
    std::vector<uint8_t>::iterator cit;
    bool written;
    bool readOnly;
    std::list<Node>::iterator it;

public:
    MockFileImp(Node n, bool ro, size_t size = 4096, const uint8_t* initBuf = nullptr, size_t initSize = 0) : node(n), readOn(ro), written(false) {
        if(size) {
            // reserve memory space for the "file"
            node.content.reserve(size);
        }
        if(initBuf) {
            std::copy(initBuf, initBuf + initSize, node.content.begin());
        }
        cit = node.content.begin();
    }
    MockFileImp(Node n, bool ro, size_t size = 4096, const char* initBuf)
            : MockFileImp(n, size, static_cast<const uint8_t*>(initBuf), strlen(initBuf)) { }
    MockFileImp(Node n, bool ro, size_t size = 4096, const String initBuf)
            : MockFileImp(n, size, static_cast<const uint8_t*>(initBuf.c_str()), initBuf.len()) { }

    virtual ~FileImpl() {
        close();
    }

    virtual size_t write(const uint8_t *buf, size_t size) {
        if(readOnly)
            return 0;

        for(size_t i=0;i<size;i++){
            node.content.push
        }
    }

    virtual size_t read(uint8_t* buf, size_t size) {
        // basic check if we reach the end of the file
        const size_t cap = std::distance(cit, node.content.end());
        const size_t rsize = std::min(size, cap);

        std::copy(cit, cit + rsize, buf);,
        cit += rsize;
        return rsize;
    }

    virtual void flush() { }

    virtual bool seek(uint32_t pos, SeekMode mode) {

    }

    virtual size_t position() const {
        return std::distance(node.content.begin(), cit);
    }
    virtual size_t size() const = 0;
    virtual bool setBufferSize(size_t size) = 0;
    
    virtual void close() {
        delete [] node.content;
        wpos = rpos = nullptr;
        written = false;
    }

    virtual time_t getLastWrite() = 0;
    virtual const char* name() const = 0;
    virtual boolean isDirectory(void) = 0;
    virtual FileImplPtr openNextFile(const char* mode) = 0;
    virtual boolean seekDir(long position);
    virtual String getNextFileName(void);
    virtual void rewindDirectory(void) = 0;
    virtual operator bool() = 0;
}

} // namespace mockfs


#define MOCK_FS
struct Node {
    ~Node() { content.clear(); }
    String fullPath{""};
    String name() const {
        return fullPath.substring(fullPath.lastIndexOf('/') + 1);
    }
    bool valid{false};
    bool isDir{false};
    size_t size{0};
    std::list<Node> content{std::list<Node>()};
};

class File
{
public:
    File(Node n = Node()) : node(n) {
        if(n.isDir)
            it = node.content.begin();
    }
    virtual ~File() {
        node.content.clear();
    }

    size_t size() const {
        if(node.isDir)
            return 0;
        return node.size;
    }

    operator bool() const {
        return node.valid;
    }

    const char* name() const {
        return node.fullPath.c_str();
    }

    boolean isDirectory(void) {
        return node.isDir;
    }

    File openNextFile(const char* mode = "r") {
        if(!node.isDir)
            return File();

        if(it == node.content.end())
            return File();
        auto i = it;
        it++;
        return File(*i);
    }

    void rewindDirectory(void) {
        if(node.isDir)
            it = node.content.begin();
    }

    Node node;
    std::list<Node>::iterator it;
};

class WriteFile : public File {
public:
    WriteFile(Node n = Node()) : File(n), allocCount(1), fileSize(0), data(new uint8_t[allocCount * allocSize]) {
    }
    virtual ~WriteFile() {
        delete data;
    }

    size_t allocCount;
    size_t fileSize;
    uint8_t *data;
    static constexpr size_t allocSize=1024;
};

class ReadFile : public File {
public:
    ReadFile(Node n = Node(), const uint8_t *data) : File(n), data(data), pos(0) {
    }
    virtual ~ReadFile() { }

    const uint8_t * data;
    size_t pos;
};