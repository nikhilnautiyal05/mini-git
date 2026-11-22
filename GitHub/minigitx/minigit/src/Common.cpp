#include "Common.h"
#include <cstdlib>
#include <cstdio>
#include <cctype>

namespace util {

std::string toStr(long long v) {
    std::ostringstream o;
    o << v;
    return o.str();
}

long toInt(const std::string& s, long fallback) {
    if (s.empty()) return fallback;
    char* end = 0;
    long v = std::strtol(s.c_str(), &end, 10);
    if (*end != '\0') return fallback;
    return v;
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == sep) { out.push_back(cur); cur.clear(); }
        else cur += s[i];
    }
    out.push_back(cur);
    return out;
}

std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> out;
    if (s.empty()) return out;
    out = split(s, '\n');
    if (!out.empty() && out.back().empty()) out.pop_back();
    return out;
}

std::string join(const std::vector<std::string>& parts, const std::string& sep) {
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) out += sep;
        out += parts[i];
    }
    return out;
}

bool startsWith(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

bool endsWith(const std::string& s, const std::string& suf) {
    return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}

std::string rtrim(const std::string& s) {
    size_t e = s.size();
    while (e > 0 && std::isspace((unsigned char)s[e - 1])) --e;
    return s.substr(0, e);
}

std::string trim(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && std::isspace((unsigned char)s[b])) ++b;
    return rtrim(s.substr(b));
}

std::string lower(const std::string& s) {
    std::string o = s;
    for (size_t i = 0; i < o.size(); ++i) o[i] = (char)std::tolower((unsigned char)o[i]);
    return o;
}

std::string jsonEscape(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) { char buf[8]; std::sprintf(buf, "\\u%04x", c); out += buf; }
                else out += (char)c;
        }
    }
    return out;
}

std::string shortHash(const std::string& h) { return h.substr(0, 7); }

} // namespace util
