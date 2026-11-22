#ifndef OBJECTSTORE_H
#define OBJECTSTORE_H

#include "Objects.h"

// Content-addressable object database: objects/ab/cdef... (zlib-compressed).
// Writing the same content twice stores it once (the hash is the filename).
class ObjectStore {
public:
    explicit ObjectStore(const std::string& dir) : dir_(dir) {}

    std::string write(const Object& obj);                                          // returns hash
    std::string writeRaw(const std::string& type, const std::string& body);
    bool exists(const std::string& hash) const;
    void readRaw(const std::string& hash, std::string& type, std::string& body) const;

    std::unique_ptr<Object> load(const std::string& hash) const;
    Commit loadCommit(const std::string& hash) const;
    Tree loadTree(const std::string& hash) const;
    std::string blobContent(const std::string& hash) const;

    // Expand an abbreviated hash (>= 4 chars). Returns "" if nothing matches.
    std::string resolvePrefix(const std::string& prefix) const;
    std::vector<std::string> allHashes() const;

private:
    std::string dir_;
    std::string pathFor(const std::string& hash) const;
};

#endif
