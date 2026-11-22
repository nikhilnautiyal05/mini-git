#ifndef UNDO_H
#define UNDO_H

#include "Common.h"
struct RepoState {
    std::string head;                              // raw HEAD content
    std::map<std::string, std::string> branches;   // branch name -> commit hash
};
//records operations
struct UndoEntry {
    std::string kind;
    std::string desc;
    RepoState state;
};
//stack used
// Two stacks (undo / redo) saved as text files.
namespace UndoLog {
std::vector<UndoEntry> load(const std::string& path);
void save(const std::string& path, const std::vector<UndoEntry>& entries);
}

#endif
