#ifndef COMMANDS_H
#define COMMANDS_H

#include "Repository.h"
#include <memory>
#include <ostream>

// Command pattern: every CLI command is a small class with run().
// main.cpp looks commands up in a table, so adding a command = adding a class.
class Command {
public:
    virtual ~Command() {}
    virtual std::string name() const = 0;
    virtual bool needsRepo() const { return true; }
    // "refs" or "tree" => the operation is recorded for undo/redo (if it changed anything)
    virtual std::string undoKind() const { return ""; }
    virtual int run(Repository& repo, const std::vector<std::string>& args) = 0;
};

typedef std::map<std::string, std::shared_ptr<Command> > CommandTable;

CommandTable buildCommandTable();
int runCommand(const std::string& name, const std::vector<std::string>& args,
               bool searchUpward, std::ostream& err);
void printUsage();

#endif
