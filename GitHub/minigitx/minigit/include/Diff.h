#ifndef DIFF_H
#define DIFF_H

#include <string>
#include <vector>

// Line-based diff using the Longest Common Subsequence (dynamic programming).
namespace Diff {
bool isBinary(const std::string& content);

// Unified-diff hunks ("@@ -a,b +c,d @@ ...") for two line lists.
std::string unified(const std::vector<std::string>& a, const std::vector<std::string>& b, int context = 3);

// Full per-file diff with Git-style header.
std::string fileDiff(const std::string& path,
                     bool existedBefore, const std::string& oldHash, const std::string& oldContent,
                     bool existsNow,     const std::string& newHash, const std::string& newContent);
}

#endif
