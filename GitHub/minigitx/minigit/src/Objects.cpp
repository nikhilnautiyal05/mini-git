#include "Objects.h"
#include "Sha1.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>

// ----------------------------------------------------------------- Object ---
std::string Object::hashOf(const std::string& type, const std::string& body) {
    std::string full = type + " " + util::toStr((long long)body.size());
    full += '\0';
    full += body;
    return Sha1::hex(full);
}

std::string Object::id() const { return hashOf(type(), serialize()); }

std::unique_ptr<Object> Object::parse(const std::string& type, const std::string& body) {
    if (type == "blob")   return std::unique_ptr<Object>(new Blob(body));
    if (type == "tree")   { Tree* t = new Tree(Tree::parse(body));     return std::unique_ptr<Object>(t); }
    if (type == "commit") { Commit* c = new Commit(Commit::parse(body)); return std::unique_ptr<Object>(c); }
    throw MiniGitError("unknown object type '" + type + "'");
}

// ------------------------------------------------------------------- Tree ---
// Git sorts tree entries bytewise, comparing directories as if their name
// ended with '/'. Matching this exactly is what makes our hashes equal Git's.
static bool entryLess(const TreeEntry& a, const TreeEntry& b) {
    std::string x = a.name + (a.isDir() ? "/" : "");
    std::string y = b.name + (b.isDir() ? "/" : "");
    return x < y;
}

std::string Tree::serialize() const {
    std::vector<TreeEntry> sorted = entries;
    std::sort(sorted.begin(), sorted.end(), entryLess);
    std::string out;
    for (size_t i = 0; i < sorted.size(); ++i) {
        out += sorted[i].mode + " " + sorted[i].name;
        out += '\0';
        out += Sha1::toRaw(sorted[i].hash);
    }
    return out;
}

Tree Tree::parse(const std::string& body) {
    Tree t;
    size_t p = 0;
    while (p < body.size()) {
        size_t sp = body.find(' ', p);
        if (sp == std::string::npos) throw MiniGitError("corrupt tree object");
        size_t nul = body.find('\0', sp);
        if (nul == std::string::npos || nul + 21 > body.size()) throw MiniGitError("corrupt tree object");
        TreeEntry e;
        e.mode = body.substr(p, sp - p);
        e.name = body.substr(sp + 1, nul - sp - 1);
        e.hash = Sha1::toHex(body.substr(nul + 1, 20));
        t.entries.push_back(e);
        p = nul + 21;
    }
    return t;
}

// ----------------------------------------------------------------- Commit ---
std::string Commit::serialize() const {
    std::string out = "tree " + tree + "\n";
    for (size_t i = 0; i < parents.size(); ++i) out += "parent " + parents[i] + "\n";
    out += "author " + author + "\n";
    out += "committer " + committer + "\n\n";
    out += message;
    return out;
}

Commit Commit::parse(const std::string& body) {
    Commit c;
    size_t p = 0;
    while (p < body.size()) {
        size_t e = body.find('\n', p);
        if (e == std::string::npos) e = body.size();
        std::string line = body.substr(p, e - p);
        p = e + 1;
        if (line.empty()) break;                      // blank line: message follows
        if (line[0] == ' ') continue;                 // continuation (e.g. gpgsig)
        size_t sp = line.find(' ');
        if (sp == std::string::npos) continue;
        std::string key = line.substr(0, sp), val = line.substr(sp + 1);
        if (key == "tree") c.tree = val;
        else if (key == "parent") c.parents.push_back(val);
        else if (key == "author") c.author = val;
        else if (key == "committer") c.committer = val;
    }
    if (p < body.size()) c.message = body.substr(p);
    if (c.tree.empty()) throw MiniGitError("corrupt commit object (no tree)");
    return c;
}

std::string Commit::authorName() const {
    size_t lt = author.find(" <");
    return lt == std::string::npos ? author : author.substr(0, lt);
}

std::string Commit::authorEmail() const {
    size_t lt = author.find('<'), gt = author.find('>');
    if (lt == std::string::npos || gt == std::string::npos || gt < lt) return "";
    return author.substr(lt + 1, gt - lt - 1);
}

long long Commit::timestamp() const {
    size_t gt = author.rfind('>');
    if (gt == std::string::npos) return 0;
    std::vector<std::string> parts = util::split(util::trim(author.substr(gt + 1)), ' ');
    return parts.empty() ? 0 : std::atoll(parts[0].c_str());
}

std::string Commit::tz() const {
    size_t gt = author.rfind('>');
    if (gt == std::string::npos) return "+0000";
    std::vector<std::string> parts = util::split(util::trim(author.substr(gt + 1)), ' ');
    return parts.size() < 2 ? "+0000" : parts[1];
}

static std::string pad2(int v) {
    std::string s = util::toStr(v);
    return s.size() < 2 ? "0" + s : s;
}

std::string Commit::dateString() const {
    static const char* wd[]  = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    static const char* mon[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    std::string z = tz();
    int sign = (!z.empty() && z[0] == '-') ? -1 : 1;
    int hh = z.size() >= 3 ? std::atoi(z.substr(1, 2).c_str()) : 0;
    int mm = z.size() >= 5 ? std::atoi(z.substr(3, 2).c_str()) : 0;
    time_t t = (time_t)(timestamp() + sign * (hh * 3600 + mm * 60));
    struct tm g = *gmtime(&t);
    std::ostringstream o;
    o << wd[g.tm_wday] << ' ' << mon[g.tm_mon] << ' ' << g.tm_mday << ' '
      << pad2(g.tm_hour) << ':' << pad2(g.tm_min) << ':' << pad2(g.tm_sec) << ' '
      << (g.tm_year + 1900) << ' ' << z;
    return o.str();
}

std::string Commit::summary() const {
    size_t nl = message.find('\n');
    return nl == std::string::npos ? message : message.substr(0, nl);
}
