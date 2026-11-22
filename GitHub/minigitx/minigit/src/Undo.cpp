#include "Undo.h"
#include "FileUtil.h"

namespace UndoLog {

std::vector<UndoEntry> load(const std::string& path) {
    std::vector<UndoEntry> out;
    if (!fs::isFile(path)) return out;
    std::vector<std::string> lines = util::splitLines(fs::readFile(path));
    for (size_t i = 0; i < lines.size(); ++i) {
        std::vector<std::string> f = util::split(lines[i], '\t');
        if (f.size() < 4) continue;
        UndoEntry e;
        e.kind = f[0];
        e.desc = f[1];
        e.state.head = f[2];
        std::vector<std::string> bs = util::split(f[3], ';');
        for (size_t j = 0; j < bs.size(); ++j) {
            size_t eq = bs[j].find('=');
            if (eq == std::string::npos) continue;
            e.state.branches[bs[j].substr(0, eq)] = bs[j].substr(eq + 1);
        }
        out.push_back(e);
    }
    return out;
}

void save(const std::string& path, const std::vector<UndoEntry>& entries) {
    std::string out;
    for (size_t i = 0; i < entries.size(); ++i) {
        const UndoEntry& e = entries[i];
        std::string bs;
        for (std::map<std::string, std::string>::const_iterator it = e.state.branches.begin();
             it != e.state.branches.end(); ++it) {
            if (!bs.empty()) bs += ";";
            bs += it->first + "=" + it->second;
        }
        out += e.kind + "\t" + e.desc + "\t" + e.state.head + "\t" + bs + "\n";
    }
    fs::writeFile(path, out);
}

} // namespace UndoLog
