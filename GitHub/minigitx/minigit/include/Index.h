#ifndef INDEX_H
#define INDEX_H

#include "Common.h"

// The staging area: path -> (mode, blob hash), kept in a sorted map
// (red-black tree). On disk it uses real Git's binary index format
// ("DIRC", version 2), so real Git can read it too.
class Index {
public:
    explicit Index(const std::string& path) : path_(path) {}
    void load();
    void save() const;
    FileMap& entries() { return entries_; }
    const FileMap& entries() const { return entries_; }

private:
    std::string path_;
    FileMap entries_;
};

#endif
