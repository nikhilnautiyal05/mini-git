#ifndef REFS_H
#define REFS_H

#include "Common.h"

// HEAD and branches. A branch is just a file holding a commit hash.
//   HEAD content:  "ref: refs/heads/master"   (on a branch)
//              or  "<40-hex hash>"            (detached HEAD)
class Refs {
public:
    explicit Refs(const std::string& repoDir) : dir_(repoDir) {}

    std::string readHead() const;
    void writeHeadRaw(const std::string& content);
    bool isDetached() const;
    std::string currentBranch() const;       // "" when detached
    std::string headCommit() const;          // "" when the branch has no commits yet
    void setHeadToBranch(const std::string& name);
    void setHeadDetached(const std::string& hash);
    void updateHead(const std::string& hash);   // move the current branch (or detached HEAD)

    bool branchExists(const std::string& name) const;
    std::string branchTip(const std::string& name) const;   // "" if missing
    void setBranch(const std::string& name, const std::string& hash);
    void deleteBranch(const std::string& name);
    std::vector<std::string> branches() const;               // sorted

    static bool validName(const std::string& name);

private:
    std::string dir_;
    void collect(const std::string& dir, const std::string& prefix, std::vector<std::string>& out) const;
};

#endif
