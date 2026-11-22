#ifndef REPOSITORY_H
#define REPOSITORY_H

#include "Common.h"
#include "ObjectStore.h"
#include "Refs.h"
#include "Index.h"
#include "Undo.h"

struct CommitInfo {
    std::string hash;
    Commit commit;
};

struct StatusResult {
    std::vector<std::pair<char, std::string> > staged;     // 'A' 'M' 'D'  (index vs HEAD)
    std::vector<std::pair<char, std::string> > unstaged;   // 'M' 'D'      (folder vs index)
    std::vector<std::string> untracked;
    bool merging;
    StatusResult() : merging(false) {}
    bool clean() const { return staged.empty() && unstaged.empty(); }
};

struct MergeResult {
    std::string message;
    bool conflict;
    std::vector<std::string> conflicts;
    MergeResult() : conflict(false) {}
};

// The heart of the program: ties together the object store, refs and index and
// implements every high-level operation. Commands (see Commands.cpp) only
// parse arguments and print results.
class Repository {
public:
    static const std::string DIR;              // ".minigit"
    static const std::string DEFAULT_BRANCH;   // "master"

    // Walk up from the current folder until a .minigit folder is found, then
    // chdir() into that root. 'prefix' receives the path from root to the
    // folder the user was in (so relative path arguments still work).
    static bool locate(std::string& prefix);

    explicit Repository(const std::string& prefix = "");

    bool init();                               // returns true if it already existed
    ObjectStore& store() { return store_; }
    Refs& refs() { return refs_; }
    Index& index() { return index_; }
    std::string toRepoPath(const std::string& userPath) const;

    // configuration (.minigit/config, "key=value" lines)
    std::string getConfig(const std::string& key) const;
    void setConfig(const std::string& key, const std::string& value);

    // core operations
    void add(const std::vector<std::string>& paths);
    std::string commit(const std::string& message);
    StatusResult status();
    std::string checkout(const std::string& target, bool createBranch);
    MergeResult merge(const std::string& branch);
    void abortMerge();
    bool merging() const;
    std::string mergeMessage() const;

    // history & revisions
    const Commit& getCommit(const std::string& hash);
    std::vector<CommitInfo> history(const std::string& start, int limit);   // newest first
    std::vector<CommitInfo> allHistory();                                   // every branch
    std::string resolveRev(const std::string& rev);       // HEAD, HEAD~2, branch, short/full hash
    FileMap commitFiles(const std::string& commitHash);   // flatten a commit's tree
    FileMap headFiles() { return commitFiles(refs_.headCommit()); }
    bool isAncestor(const std::string& maybeAncestor, const std::string& of);
    std::string mergeBase(const std::string& a, const std::string& b);

    // working folder helpers
    std::vector<std::string> workingFiles();
    std::string hashFile(const std::string& path);
    bool isIgnored(const std::string& relPath) const;

    // undo / redo
    RepoState captureState() const;
    void recordOperation(const std::string& kind, const std::string& desc, const RepoState& before);
    std::string undo();
    std::string redo();

    // integrity check: returns number of objects checked, fills 'problems'
    int verify(std::vector<std::string>& problems);

private:
    std::string prefix_;
    ObjectStore store_;
    Refs refs_;
    Index index_;
    std::vector<std::string> ignore_;
    std::map<std::string, Commit> commitCache_;

    std::vector<CommitInfo> walkOrdered(const std::vector<std::string>& starts, int limit);
    void loadIgnore();
    void walk(const std::string& dir, std::vector<std::string>& out) const;
    void stageFile(const std::string& path);
    std::string writeTree(const FileMap& files);
    void flattenTree(const std::string& treeHash, const std::string& prefix, FileMap& out);
    std::string identity() const;
    std::string createCommit(const std::string& tree, const std::vector<std::string>& parents,
                             const std::string& message);
    void ensureClean();
    void applyTree(const std::string& fromCommit, const std::string& toCommit);
    void switchTree(const std::string& fromCommit, const std::string& toCommit);
    void resetHard();
    void restoreRefs(const RepoState& st);
    void applyState(const RepoState& st, const std::string& kind);
};

#endif
