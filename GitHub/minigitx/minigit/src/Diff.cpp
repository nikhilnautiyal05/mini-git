#include "Diff.h"
#include "Common.h"
#include <algorithm>

namespace Diff {

namespace {
struct Op {
    char t;                    // ' ' same, '-' removed, '+' added
    const std::string* text;
};

std::string range(int start, int count) {
    // Git prints "start" alone when count == 1, and "start,0" for empty ranges.
    if (count == 1) return util::toStr(start + 1);
    if (count == 0) return util::toStr(start) + ",0";
    return util::toStr(start + 1) + "," + util::toStr(count);
}
}

bool isBinary(const std::string& content) {
    size_t n = std::min(content.size(), (size_t)8000);
    return content.find('\0') < n;
}

std::string unified(const std::vector<std::string>& a, const std::vector<std::string>& b, int context) {
    // 1. Trim the common prefix and suffix so the DP table stays small.
    size_t pre = 0;
    while (pre < a.size() && pre < b.size() && a[pre] == b[pre]) ++pre;
    size_t suf = 0;
    while (suf < a.size() - pre && suf < b.size() - pre &&
           a[a.size() - 1 - suf] == b[b.size() - 1 - suf]) ++suf;
    size_t n = a.size() - pre - suf, m = b.size() - pre - suf;

    std::vector<Op> ops;
    for (size_t i = 0; i < pre; ++i) { Op o = {' ', &a[i]}; ops.push_back(o); }

    // 2. LCS table over the differing middle part.
    if (n * m > 16000000UL) {                       // too large: treat as full replacement
        for (size_t i = 0; i < n; ++i) { Op o = {'-', &a[pre + i]}; ops.push_back(o); }
        for (size_t j = 0; j < m; ++j) { Op o = {'+', &b[pre + j]}; ops.push_back(o); }
    } else {
        std::vector<std::vector<int> > L(n + 1, std::vector<int>(m + 1, 0));
        for (size_t i = n; i-- > 0;)
            for (size_t j = m; j-- > 0;)
                L[i][j] = (a[pre + i] == b[pre + j]) ? L[i + 1][j + 1] + 1
                                                     : std::max(L[i + 1][j], L[i][j + 1]);
        size_t i = 0, j = 0;
        while (i < n && j < m) {
            if (a[pre + i] == b[pre + j])      { Op o = {' ', &a[pre + i]}; ops.push_back(o); ++i; ++j; }
            else if (L[i + 1][j] >= L[i][j + 1]) { Op o = {'-', &a[pre + i]}; ops.push_back(o); ++i; }
            else                               { Op o = {'+', &b[pre + j]}; ops.push_back(o); ++j; }
        }
        while (i < n) { Op o = {'-', &a[pre + i]}; ops.push_back(o); ++i; }
        while (j < m) { Op o = {'+', &b[pre + j]}; ops.push_back(o); ++j; }
    }
    for (size_t i = 0; i < suf; ++i) { Op o = {' ', &a[a.size() - suf + i]}; ops.push_back(o); }

    // 3. Line positions before each op, then group changes into hunks.
    std::vector<int> aPos(ops.size() + 1, 0), bPos(ops.size() + 1, 0);
    for (size_t k = 0; k < ops.size(); ++k) {
        aPos[k + 1] = aPos[k] + (ops[k].t != '+' ? 1 : 0);
        bPos[k + 1] = bPos[k] + (ops[k].t != '-' ? 1 : 0);
    }

    std::vector<std::pair<size_t, size_t> > ranges;
    for (size_t k = 0; k < ops.size(); ++k) {
        if (ops[k].t == ' ') continue;
        size_t s = k > (size_t)context ? k - context : 0;
        size_t e = std::min(k + context + 1, ops.size());
        if (!ranges.empty() && s <= ranges.back().second) ranges.back().second = std::max(ranges.back().second, e);
        else ranges.push_back(std::make_pair(s, e));
    }

    std::string out;
    for (size_t r = 0; r < ranges.size(); ++r) {
        size_t s = ranges[r].first, e = ranges[r].second;
        out += "@@ -" + range(aPos[s], aPos[e] - aPos[s]) + " +" + range(bPos[s], bPos[e] - bPos[s]) + " @@\n";
        for (size_t k = s; k < e; ++k) out += std::string(1, ops[k].t) + *ops[k].text + "\n";
    }
    return out;
}

std::string fileDiff(const std::string& path,
                     bool existedBefore, const std::string& oldHash, const std::string& oldContent,
                     bool existsNow,     const std::string& newHash, const std::string& newContent) {
    const std::string zero = "0000000";
    std::string out = "diff --git a/" + path + " b/" + path + "\n";
    if (!existedBefore) out += "new file mode 100644\n";
    if (!existsNow)     out += "deleted file mode 100644\n";
    out += "index " + (existedBefore ? oldHash.substr(0, 7) : zero) + ".." +
           (existsNow ? newHash.substr(0, 7) : zero) + ((existedBefore && existsNow) ? " 100644" : "") + "\n";

    if (isBinary(oldContent) || isBinary(newContent)) {
        out += "Binary files " + (existedBefore ? "a/" + path : std::string("/dev/null")) + " and " +
               (existsNow ? "b/" + path : std::string("/dev/null")) + " differ\n";
        return out;
    }
    std::string body = unified(util::splitLines(oldContent), util::splitLines(newContent), 3);
    if (body.empty() && existedBefore && existsNow) return "";     // identical text
    out += "--- " + (existedBefore ? "a/" + path : std::string("/dev/null")) + "\n";
    out += "+++ " + (existsNow ? "b/" + path : std::string("/dev/null")) + "\n";
    return out + body;
}

} // namespace Diff
