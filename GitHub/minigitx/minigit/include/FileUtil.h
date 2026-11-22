#ifndef FILEUTIL_H
#define FILEUTIL_H

#include <string>
#include <vector>

// Small portable file helpers (no std::filesystem: it needs C++17).
namespace fs {
bool exists(const std::string& path);
bool isDir(const std::string& path);
bool isFile(const std::string& path);
std::string readFile(const std::string& path);                       // throws MiniGitError
void writeFile(const std::string& path, const std::string& data);    // creates parent folders
void makeDirs(const std::string& path);
bool removeFile(const std::string& path);
// Remove now-empty parent folders of 'path', stopping at 'stopAt' (or the top).
void removeEmptyParents(const std::string& path, const std::string& stopAt = "");
std::vector<std::string> listDir(const std::string& path);           // names only, sorted
std::string cwd();
bool changeDir(const std::string& path);
std::string normalize(const std::string& path);                      // "a/./b/../c" -> "a/c"
// Validates a user-typed file name: must stay inside the project folder and must
// not touch .minigit/.git. Returns the cleaned relative path, or throws MiniGitError.
std::string safeRelativePath(const std::string& raw);
}

#endif
