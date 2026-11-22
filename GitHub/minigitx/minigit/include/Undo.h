#ifndef UNDO_H
#define UNDO_H

#include "Common.h"

// A snapshot of where HEAD and every branch pointed at one moment.
struct RepoState {
    std::string head;                              // raw HEAD content
    std::map<std::string, std::string> branches;   // branch name -> commit hash
};

// One recorded operation. kind:
//   "refs" = only branch pointers move (commit, branch create/delete)
//   "tree" = the working folder changes too (checkout, merge)
struct UndoEntry {
    std::string kind;
    std::string desc;
    RepoState state;
};

// Two stacks (undo / redo) saved as text files.
namespace UndoLog {
std::vector<UndoEntry> load(const std::string& path);
void save(const std::string& path, const std::vector<UndoEntry>& entries);
}

#endif
