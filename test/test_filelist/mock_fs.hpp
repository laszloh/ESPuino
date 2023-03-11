#pragma once

#include <list>

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