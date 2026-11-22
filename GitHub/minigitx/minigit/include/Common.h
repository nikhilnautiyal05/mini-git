#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <stdexcept>
#include <sstream>

// Every user-facing failure is reported through this exception type.
class MiniGitError : public std::runtime_error {
public:
    explicit MiniGitError(const std::string& msg) : std::runtime_error(msg) {}
};

// One file inside a snapshot: its mode ("100644") and blob hash.
struct FileEntry {
    std::string mode;
    std::string hash;
    FileEntry() {}
    FileEntry(const std::string& m, const std::string& h) : mode(m), hash(h) {}
};

// Sorted map (red-black tree): path -> entry. Used for trees, index, snapshots.
typedef std::map<std::string, FileEntry> FileMap;

// NOTE: std::to_string / std::stoi are avoided on purpose. They are broken on
// old MinGW builds (GCC 6.x), so small helpers are provided instead.
namespace util {
std::string toStr(long long v);
long toInt(const std::string& s, long fallback = 0);
std::vector<std::string> split(const std::string& s, char sep);
std::vector<std::string> splitLines(const std::string& s);
std::string join(const std::vector<std::string>& parts, const std::string& sep);
bool startsWith(const std::string& s, const std::string& prefix);
bool endsWith(const std::string& s, const std::string& suffix);
std::string trim(const std::string& s);
std::string rtrim(const std::string& s);
std::string lower(const std::string& s);
std::string jsonEscape(const std::string& s);
std::string shortHash(const std::string& h);
}

#endif
