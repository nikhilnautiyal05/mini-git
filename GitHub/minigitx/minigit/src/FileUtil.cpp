#include "FileUtil.h"
#include "Common.h"
#include <fstream>
#include <algorithm>
#include <cstdio>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#ifdef _WIN32
  #include <direct.h>
  #define MG_MKDIR(p)     _mkdir(p)
  #define MG_RMDIR(p)     _rmdir(p)
  #define MG_GETCWD(b, n) _getcwd(b, n)
  #define MG_CHDIR(p)     _chdir(p)
#else
  #include <unistd.h>
  #define MG_MKDIR(p)     mkdir(p, 0755)
  #define MG_RMDIR(p)     rmdir(p)
  #define MG_GETCWD(b, n) getcwd(b, n)
  #define MG_CHDIR(p)     chdir(p)
#endif

namespace fs {

bool exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool isDir(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    return (st.st_mode & S_IFMT) == S_IFDIR;
}

bool isFile(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    return (st.st_mode & S_IFMT) == S_IFREG;
}

std::string readFile(const std::string& path) {
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) throw MiniGitError("cannot read file '" + path + "'");
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void makeDirs(const std::string& path) {
    std::string cur;
    std::vector<std::string> parts = util::split(path, '/');
    for (size_t i = 0; i < parts.size(); ++i) {
        if (parts[i].empty()) { if (i == 0) cur = "/"; continue; }
        if (!cur.empty() && cur[cur.size() - 1] != '/') cur += "/";
        cur += parts[i];
        if (!isDir(cur)) MG_MKDIR(cur.c_str());
    }
}

void writeFile(const std::string& path, const std::string& data) {
    size_t slash = path.find_last_of('/');
    if (slash != std::string::npos && slash > 0) makeDirs(path.substr(0, slash));
    std::ofstream out(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!out) throw MiniGitError("cannot write file '" + path + "'");
    out.write(data.data(), (std::streamsize)data.size());
    if (!out) throw MiniGitError("failed while writing '" + path + "'");
}

bool removeFile(const std::string& path) {
    return std::remove(path.c_str()) == 0;
}

void removeEmptyParents(const std::string& path, const std::string& stopAt) {
    std::string cur = path;
    while (true) {
        size_t slash = cur.find_last_of('/');
        if (slash == std::string::npos) return;
        cur = cur.substr(0, slash);
        if (cur.empty() || cur == stopAt) return;
        if (MG_RMDIR(cur.c_str()) != 0) return;   // not empty (or cannot remove): stop
    }
}

std::vector<std::string> listDir(const std::string& path) {
    std::vector<std::string> names;
    DIR* d = opendir(path.c_str());
    if (!d) return names;
    struct dirent* e;
    while ((e = readdir(d)) != 0) {
        std::string n = e->d_name;
        if (n == "." || n == "..") continue;
        names.push_back(n);
    }
    closedir(d);
    std::sort(names.begin(), names.end());
    return names;
}

std::string cwd() {
    char buf[4096];
    if (!MG_GETCWD(buf, sizeof(buf))) return "";
    std::string s = buf;
    for (size_t i = 0; i < s.size(); ++i) if (s[i] == '\\') s[i] = '/';
    return s;
}

bool changeDir(const std::string& path) { return MG_CHDIR(path.c_str()) == 0; }

std::string normalize(const std::string& path) {
    std::string p = path;
    for (size_t i = 0; i < p.size(); ++i) if (p[i] == '\\') p[i] = '/';
    std::vector<std::string> parts = util::split(p, '/');
    std::vector<std::string> stack;
    for (size_t i = 0; i < parts.size(); ++i) {
        const std::string& s = parts[i];
        if (s.empty() || s == ".") continue;
        if (s == "..") {
            if (!stack.empty() && stack.back() != "..") stack.pop_back();
            else stack.push_back("..");
        } else stack.push_back(s);
    }
    return util::join(stack, "/");
}

std::string safeRelativePath(const std::string& raw) {
    if (raw.empty() || raw[0] == '/' || raw[0] == '\\' || raw.find(':') != std::string::npos)
        throw MiniGitError("invalid file name");
    std::string p = normalize(raw);
    if (p.empty() || p == ".." || util::startsWith(p, "../")) throw MiniGitError("invalid file name");
    std::string top = p.substr(0, p.find('/'));
    if (top == ".minigit" || top == ".git") throw MiniGitError("that folder is managed by version control");
    return p;
}

} // namespace fs
