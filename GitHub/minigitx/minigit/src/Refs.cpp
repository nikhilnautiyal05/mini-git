#include "Refs.h"
#include "FileUtil.h"

static const char* HEADS = "/refs/heads/";

std::string Refs::readHead() const { return util::trim(fs::readFile(dir_ + "/HEAD")); }

void Refs::writeHeadRaw(const std::string& content) { fs::writeFile(dir_ + "/HEAD", content + "\n"); }

bool Refs::isDetached() const { return !util::startsWith(readHead(), "ref: "); }

std::string Refs::currentBranch() const {
    std::string h = readHead();
    const std::string pre = "ref: refs/heads/";
    return util::startsWith(h, pre) ? h.substr(pre.size()) : "";
}

std::string Refs::headCommit() const {
    std::string h = readHead();
    if (util::startsWith(h, "ref: ")) {
        std::string path = dir_ + "/" + h.substr(5);
        return fs::isFile(path) ? util::trim(fs::readFile(path)) : "";
    }
    return h;
}

void Refs::setHeadToBranch(const std::string& name) { writeHeadRaw("ref: refs/heads/" + name); }
void Refs::setHeadDetached(const std::string& hash) { writeHeadRaw(hash); }

void Refs::updateHead(const std::string& hash) {
    std::string b = currentBranch();
    if (b.empty()) setHeadDetached(hash);
    else setBranch(b, hash);
}

bool Refs::branchExists(const std::string& name) const {
    return fs::isFile(dir_ + HEADS + name);
}

std::string Refs::branchTip(const std::string& name) const {
    std::string p = dir_ + HEADS + name;
    return fs::isFile(p) ? util::trim(fs::readFile(p)) : "";
}

void Refs::setBranch(const std::string& name, const std::string& hash) {
    fs::writeFile(dir_ + HEADS + name, hash + "\n");
}

void Refs::deleteBranch(const std::string& name) {
    std::string p = dir_ + HEADS + name;
    fs::removeFile(p);
    fs::removeEmptyParents(p, dir_ + "/refs/heads");
}

void Refs::collect(const std::string& dir, const std::string& prefix, std::vector<std::string>& out) const {
    std::vector<std::string> names = fs::listDir(dir);
    for (size_t i = 0; i < names.size(); ++i) {
        std::string full = dir + "/" + names[i];
        if (fs::isDir(full)) collect(full, prefix + names[i] + "/", out);
        else out.push_back(prefix + names[i]);
    }
}

std::vector<std::string> Refs::branches() const {
    std::vector<std::string> out;
    collect(dir_ + "/refs/heads", "", out);
    return out;
}

bool Refs::validName(const std::string& n) {
    if (n.empty() || n[0] == '-' || n[0] == '/' || n[n.size() - 1] == '/') return false;
    if (n.find("..") != std::string::npos || n.find("//") != std::string::npos) return false;
    if (util::endsWith(n, ".lock") || n == "HEAD") return false;
    for (size_t i = 0; i < n.size(); ++i) {
        char c = n[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '.' || c == '_' || c == '-' || c == '/';
        if (!ok) return false;
    }
    return true;
}
