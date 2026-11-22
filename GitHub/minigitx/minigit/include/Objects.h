#ifndef OBJECTS_H
#define OBJECTS_H

#include "Common.h"
#include <memory>

// ---------------------------------------------------------------------------
// The three object kinds of Git's object model, as an OOP hierarchy.
//   Blob   = file content
//   Tree   = directory listing (names -> blob/tree hashes)
//   Commit = snapshot (tree) + parents + author + message
// Each object knows how to serialize itself in real Git's byte format, and its
// id is SHA-1("<type> <size>\0<body>"), exactly like Git.
// ---------------------------------------------------------------------------
class Object {
public:
    virtual ~Object() {}
    virtual std::string type() const = 0;
    virtual std::string serialize() const = 0;

    std::string id() const;                                                   // hash of this object
    static std::string hashOf(const std::string& type, const std::string& body);
    static std::unique_ptr<Object> parse(const std::string& type, const std::string& body);
};

class Blob : public Object {
public:
    std::string data;
    Blob() {}
    explicit Blob(const std::string& d) : data(d) {}
    std::string type() const override { return "blob"; }
    std::string serialize() const override { return data; }
};

struct TreeEntry {
    std::string mode;   // "100644" file, "40000" directory
    std::string name;
    std::string hash;   // 40 hex chars
    bool isDir() const { return mode == "40000"; }
};

class Tree : public Object {
public:
    std::vector<TreeEntry> entries;
    std::string type() const override { return "tree"; }
    std::string serialize() const override;           // sorts entries the way Git does
    static Tree parse(const std::string& body);
};

class Commit : public Object {
public:
    std::string tree;
    std::vector<std::string> parents;
    std::string author;       // "Name <email> 1700000000 +0530"
    std::string committer;
    std::string message;      // ends with '\n'

    std::string type() const override { return "commit"; }
    std::string serialize() const override;
    static Commit parse(const std::string& body);

    std::string authorName() const;
    std::string authorEmail() const;
    long long timestamp() const;
    std::string tz() const;
    std::string dateString() const;     // "Sat Nov 1 14:20:00 2026 +0530"
    std::string summary() const;        // first line of the message
};

#endif
