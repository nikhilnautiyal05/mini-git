#include "Index.h"
#include "FileUtil.h"
#include "Sha1.h"
#include <cstdlib>
#include <cstdio>

// File layout (all numbers big-endian):
//   "DIRC" | version=2 | entry count
//   per entry: ctime(8) mtime(8) dev ino mode uid gid size (4 each) | sha1(20)
//              | flags(2: name length) | path | 1-8 NUL bytes (pad to 8)
//   trailer: SHA-1 of everything above
// We store zero for the file-stat fields; Git then simply re-checks content.

static void put32(std::string& out, unsigned long v) {
    out += (char)((v >> 24) & 0xFF);
    out += (char)((v >> 16) & 0xFF);
    out += (char)((v >> 8) & 0xFF);
    out += (char)(v & 0xFF);
}

static unsigned long get32(const std::string& s, size_t p) {
    return ((unsigned long)(unsigned char)s[p] << 24) | ((unsigned long)(unsigned char)s[p + 1] << 16) |
           ((unsigned long)(unsigned char)s[p + 2] << 8) | (unsigned long)(unsigned char)s[p + 3];
}

void Index::load() {
    entries_.clear();
    if (!fs::isFile(path_)) return;
    std::string d = fs::readFile(path_);
    if (d.empty()) return;
    if (d.size() < 32 || d.compare(0, 4, "DIRC") != 0) throw MiniGitError("index file is corrupt");
    if (Sha1::raw(d.substr(0, d.size() - 20)) != d.substr(d.size() - 20))
        throw MiniGitError("index file is corrupt (checksum mismatch)");
    unsigned long count = get32(d, 8);
    size_t p = 12;
    for (unsigned long i = 0; i < count; ++i) {
        if (p + 62 > d.size()) throw MiniGitError("index file is truncated");
        unsigned long mode = get32(d, p + 24);
        std::string hash = Sha1::toHex(d.substr(p + 40, 20));
        size_t nameLen = ((unsigned char)d[p + 60] << 8 | (unsigned char)d[p + 61]) & 0x0FFF;
        size_t nameStart = p + 62;
        if (nameLen == 0x0FFF) nameLen = d.find('\0', nameStart) - nameStart;
        std::string name = d.substr(nameStart, nameLen);
        char buf[16];
        std::sprintf(buf, "%lo", mode);
        entries_[name] = FileEntry(buf, hash);
        p += (62 + nameLen + 8) & ~(size_t)7;     // padding is relative to the entry start
    }
}

void Index::save() const {
    std::string out = "DIRC";
    put32(out, 2);
    put32(out, (unsigned long)entries_.size());
    for (FileMap::const_iterator it = entries_.begin(); it != entries_.end(); ++it) {
        size_t start = out.size();
        for (int k = 0; k < 6; ++k) put32(out, 0);                    // ctime, mtime, dev, ino
        put32(out, std::strtoul(it->second.mode.c_str(), 0, 8));       // mode
        for (int k = 0; k < 3; ++k) put32(out, 0);                    // uid, gid, size
        out += Sha1::toRaw(it->second.hash);
        size_t len = it->first.size() < 0x0FFF ? it->first.size() : 0x0FFF;
        out += (char)((len >> 8) & 0xFF);
        out += (char)(len & 0xFF);
        out += it->first;
        size_t entryLen = (62 + it->first.size() + 8) & ~(size_t)7;
        out.append(start + entryLen - out.size(), '\0');
    }
    out += Sha1::raw(out);
    fs::writeFile(path_, out);
}
