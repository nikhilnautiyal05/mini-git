#include "ObjectStore.h"
#include "FileUtil.h"
#include "Zlib.h"
#include "Sha1.h"

std::string ObjectStore::pathFor(const std::string& hash) const {
    return dir_ + "/" + hash.substr(0, 2) + "/" + hash.substr(2);
}

std::string ObjectStore::writeRaw(const std::string& type, const std::string& body) {
    std::string hash = Object::hashOf(type, body);
    std::string path = pathFor(hash);
    if (fs::isFile(path)) return hash;                    // already stored (deduplication)

    std::string full = type + " " + util::toStr((long long)body.size());
    full += '\0';
    full += body;
    fs::writeFile(path, Zlib::deflateStored(full));
    return hash;
}

std::string ObjectStore::write(const Object& obj) {
    return writeRaw(obj.type(), obj.serialize());
}

bool ObjectStore::exists(const std::string& hash) const {
    return hash.size() == 40 && fs::isFile(pathFor(hash));
}

void ObjectStore::readRaw(const std::string& hash, std::string& type, std::string& body) const {
    if (hash.size() != 40 || !fs::isFile(pathFor(hash)))
        throw MiniGitError("object " + hash + " not found");
    std::string full = Zlib::inflate(fs::readFile(pathFor(hash)));
    size_t nul = full.find('\0');
    if (nul == std::string::npos) throw MiniGitError("corrupt object " + hash + " (no header)");
    std::string header = full.substr(0, nul);
    size_t sp = header.find(' ');
    if (sp == std::string::npos) throw MiniGitError("corrupt object " + hash + " (bad header)");
    type = header.substr(0, sp);
    long size = util::toInt(header.substr(sp + 1), -1);
    body = full.substr(nul + 1);
    if (size < 0 || (size_t)size != body.size())
        throw MiniGitError("corrupt object " + hash + " (size mismatch)");
}

std::unique_ptr<Object> ObjectStore::load(const std::string& hash) const {
    std::string type, body;
    readRaw(hash, type, body);
    return Object::parse(type, body);
}

Commit ObjectStore::loadCommit(const std::string& hash) const {
    std::string type, body;
    readRaw(hash, type, body);
    if (type != "commit") throw MiniGitError(hash + " is a " + type + ", not a commit");
    return Commit::parse(body);
}

Tree ObjectStore::loadTree(const std::string& hash) const {
    std::string type, body;
    readRaw(hash, type, body);
    if (type != "tree") throw MiniGitError(hash + " is a " + type + ", not a tree");
    return Tree::parse(body);
}

std::string ObjectStore::blobContent(const std::string& hash) const {
    std::string type, body;
    readRaw(hash, type, body);
    if (type != "blob") throw MiniGitError(hash + " is a " + type + ", not a blob");
    return body;
}

std::string ObjectStore::resolvePrefix(const std::string& prefix) const {
    std::string p = util::lower(prefix);
    if (p.size() == 40) return exists(p) ? p : "";
    if (p.size() < 4 || p.size() > 40) return "";
    std::vector<std::string> files = fs::listDir(dir_ + "/" + p.substr(0, 2));
    std::string found;
    int matches = 0;
    for (size_t i = 0; i < files.size(); ++i) {
        std::string full = p.substr(0, 2) + files[i];
        if (full.size() == 40 && util::startsWith(full, p)) { found = full; ++matches; }
    }
    if (matches > 1) throw MiniGitError("short hash '" + prefix + "' is ambiguous");
    return found;
}

std::vector<std::string> ObjectStore::allHashes() const {
    std::vector<std::string> out;
    std::vector<std::string> dirs = fs::listDir(dir_);
    for (size_t i = 0; i < dirs.size(); ++i) {
        if (dirs[i].size() != 2) continue;
        std::vector<std::string> files = fs::listDir(dir_ + "/" + dirs[i]);
        for (size_t j = 0; j < files.size(); ++j)
            if (files[j].size() == 38) out.push_back(dirs[i] + files[j]);
    }
    return out;
}
