#include "Commands.h"
#include "FileUtil.h"
#include "Diff.h"
#include "Trie.h"
#include "Server.h"
#include <iostream>
#include <queue>
#include <algorithm>
#include <cctype>

using std::string;
using std::vector;
using std::set;
using std::cout;

// ---------------------------------------------------------- arg helpers -----
static bool hasFlag(const vector<string>& a, const string& f) {
    return std::find(a.begin(), a.end(), f) != a.end();
}

static string optValue(const vector<string>& a, const string& f) {
    for (size_t i = 0; i + 1 < a.size(); ++i) if (a[i] == f) return a[i + 1];
    return "";
}

static vector<string> positionals(const vector<string>& a, const set<string>& valueFlags) {
    vector<string> out;
    for (size_t i = 0; i < a.size(); ++i) {
        if (valueFlags.count(a[i])) { ++i; continue; }
        if (a[i].size() > 1 && a[i][0] == '-') continue;
        out.push_back(a[i]);
    }
    return out;
}

static const set<string>& noValueFlags() { static set<string> s; return s; }

static string statusWord(char c) {
    if (c == 'A') return "new file:   ";
    if (c == 'D') return "deleted:    ";
    return "modified:   ";
}

// ================================================================== init =====
class InitCommand : public Command {
public:
    string name() const override { return "init"; }
    bool needsRepo() const override { return false; }
    int run(Repository& repo, const vector<string>&) override {
        bool existed = repo.init();
        cout << (existed ? "Reinitialized existing" : "Initialized empty") << " Minigit repository in "
             << fs::cwd() << "/" << Repository::DIR << "/\n";
        return 0;
    }
};

// ================================================================= config ====
class ConfigCommand : public Command {
public:
    string name() const override { return "config"; }
    int run(Repository& repo, const vector<string>& args) override {
        if (args.empty() || args.size() > 2) throw MiniGitError("usage: minigit config <key> [value]   (e.g. user.name, user.email)");
        if (args.size() == 2) { repo.setConfig(args[0], args[1]); return 0; }
        string v = repo.getConfig(args[0]);
        if (v.empty()) return 1;
        cout << v << "\n";
        return 0;
    }
};

// =================================================================== add =====
class AddCommand : public Command {
public:
    string name() const override { return "add"; }
    int run(Repository& repo, const vector<string>& args) override {
        vector<string> pos = positionals(args, noValueFlags());
        if (pos.empty()) throw MiniGitError("nothing specified: use 'minigit add <file>' or 'minigit add .'");
        vector<string> paths;
        for (size_t i = 0; i < pos.size(); ++i) paths.push_back(repo.toRepoPath(pos[i]));
        repo.add(paths);
        return 0;
    }
};

// ================================================================ commit =====
class CommitCommand : public Command {
public:
    string name() const override { return "commit"; }
    string undoKind() const override { return "refs"; }
    int run(Repository& repo, const vector<string>& args) override {
        string msg = optValue(args, "-m");
        if (msg.empty() && repo.merging()) msg = repo.mergeMessage();
        if (msg.empty()) throw MiniGitError("a message is required: minigit commit -m \"your message\"");
        bool root = repo.refs().headCommit().empty();
        string hash = repo.commit(msg);
        string br = repo.refs().currentBranch();
        cout << "[" << (br.empty() ? "detached HEAD" : br) << (root ? " (root-commit) " : " ")
             << util::shortHash(hash) << "] " << repo.getCommit(hash).summary() << "\n";
        return 0;
    }
};

// ================================================================ status =====
class StatusCommand : public Command {
public:
    string name() const override { return "status"; }
    int run(Repository& repo, const vector<string>& args) override {
        StatusResult st = repo.status();
        string head = repo.refs().headCommit();
        string br = repo.refs().currentBranch();

        if (hasFlag(args, "--json")) {
            cout << "{\"branch\":\"" << util::jsonEscape(br) << "\",\"head\":\"" << head << "\",\"merging\":"
                 << (st.merging ? "true" : "false") << ",\"staged\":[";
            for (size_t i = 0; i < st.staged.size(); ++i)
                cout << (i ? "," : "") << "{\"status\":\"" << st.staged[i].first << "\",\"path\":\""
                     << util::jsonEscape(st.staged[i].second) << "\"}";
            cout << "],\"unstaged\":[";
            for (size_t i = 0; i < st.unstaged.size(); ++i)
                cout << (i ? "," : "") << "{\"status\":\"" << st.unstaged[i].first << "\",\"path\":\""
                     << util::jsonEscape(st.unstaged[i].second) << "\"}";
            cout << "],\"untracked\":[";
            for (size_t i = 0; i < st.untracked.size(); ++i)
                cout << (i ? "," : "") << "\"" << util::jsonEscape(st.untracked[i]) << "\"";
            cout << "]}\n";
            return 0;
        }

        if (br.empty()) cout << "HEAD detached at " << util::shortHash(head) << "\n";
        else cout << "On branch " << br << "\n";
        if (head.empty()) cout << "\nNo commits yet\n";
        if (st.merging) cout << "\nYou are in the middle of a merge. Fix conflicts, 'minigit add' them, then 'minigit commit'.\n";

        if (!st.staged.empty()) {
            cout << "\nChanges to be committed:\n";
            for (size_t i = 0; i < st.staged.size(); ++i)
                cout << "\t" << statusWord(st.staged[i].first) << st.staged[i].second << "\n";
        }
        if (!st.unstaged.empty()) {
            cout << "\nChanges not staged for commit:\n";
            for (size_t i = 0; i < st.unstaged.size(); ++i)
                cout << "\t" << statusWord(st.unstaged[i].first) << st.unstaged[i].second << "\n";
        }
        if (!st.untracked.empty()) {
            cout << "\nUntracked files:\n";
            for (size_t i = 0; i < st.untracked.size(); ++i) cout << "\t" << st.untracked[i] << "\n";
        }
        if (st.clean() && st.untracked.empty()) cout << "\nnothing to commit, working tree clean\n";
        return 0;
    }
};

// ==================================================================== log ====
class LogCommand : public Command {
public:
    string name() const override { return "log"; }
    int run(Repository& repo, const vector<string>& args) override {
        bool oneline = hasFlag(args, "--oneline"), json = hasFlag(args, "--json");
        set<string> vf; vf.insert("-n");
        int limit = (int)util::toInt(optValue(args, "-n"), -1);
        vector<string> pos = positionals(args, vf);

        string start = pos.empty() ? repo.refs().headCommit() : repo.resolveRev(pos[0]);
        if (start.empty()) {
            string br = repo.refs().currentBranch();
            throw MiniGitError("your current branch '" + br + "' does not have any commits yet");
        }
        vector<CommitInfo> h = repo.history(start, limit);

        if (json) {
            cout << "[";
            for (size_t i = 0; i < h.size(); ++i) {
                const Commit& c = h[i].commit;
                cout << (i ? ",\n" : "\n") << " {\"hash\":\"" << h[i].hash << "\",\"parents\":[";
                for (size_t k = 0; k < c.parents.size(); ++k) cout << (k ? "," : "") << "\"" << c.parents[k] << "\"";
                cout << "],\"author\":\"" << util::jsonEscape(c.authorName()) << "\",\"email\":\""
                     << util::jsonEscape(c.authorEmail()) << "\",\"timestamp\":" << c.timestamp()
                     << ",\"tz\":\"" << c.tz() << "\",\"message\":\"" << util::jsonEscape(util::rtrim(c.message)) << "\"}";
            }
            cout << "\n]\n";
            return 0;
        }
        for (size_t i = 0; i < h.size(); ++i) {
            const Commit& c = h[i].commit;
            if (oneline) { cout << util::shortHash(h[i].hash) << " " << c.summary() << "\n"; continue; }
            cout << "commit " << h[i].hash << "\n";
            if (c.parents.size() > 1) {
                cout << "Merge:";
                for (size_t k = 0; k < c.parents.size(); ++k) cout << " " << util::shortHash(c.parents[k]);
                cout << "\n";
            }
            cout << "Author: " << c.author.substr(0, c.author.rfind('>') + 1) << "\n";
            cout << "Date:   " << c.dateString() << "\n\n";
            vector<string> lines = util::splitLines(c.message);
            for (size_t k = 0; k < lines.size(); ++k) cout << "    " << lines[k] << "\n";
            cout << "\n";
        }
        return 0;
    }
};

// ==================================================================== diff ===
namespace {
struct Side {
    FileMap files;
    bool work;
    Side() : work(false) {}
};

string contentOf(Repository& repo, const Side& s, const string& path) {
    if (s.work) return fs::readFile(path);
    return repo.store().blobContent(s.files.find(path)->second.hash);
}

void printDiff(Repository& repo, const Side& from, const Side& to) {
    set<string> paths;
    for (FileMap::const_iterator it = from.files.begin(); it != from.files.end(); ++it) paths.insert(it->first);
    for (FileMap::const_iterator it = to.files.begin(); it != to.files.end(); ++it) paths.insert(it->first);
    for (set<string>::iterator it = paths.begin(); it != paths.end(); ++it) {
        FileMap::const_iterator f = from.files.find(*it), t = to.files.find(*it);
        bool hasF = f != from.files.end(), hasT = t != to.files.end();
        if (hasF && hasT && f->second.hash == t->second.hash) continue;
        string oc = hasF ? contentOf(repo, from, *it) : "";
        string nc = hasT ? contentOf(repo, to, *it) : "";
        cout << Diff::fileDiff(*it, hasF, hasF ? f->second.hash : "", oc, hasT, hasT ? t->second.hash : "", nc);
    }
}
}

class DiffCommand : public Command {
public:
    string name() const override { return "diff"; }
    int run(Repository& repo, const vector<string>& args) override {
        vector<string> pos = positionals(args, noValueFlags());
        Side from, to;
        if (pos.size() == 1) {                       // changes introduced by one commit
            string h = repo.resolveRev(pos[0]);
            const Commit& cm = repo.getCommit(h);
            if (!cm.parents.empty()) from.files = repo.commitFiles(cm.parents[0]);
            to.files = repo.commitFiles(h);
        } else if (pos.size() == 2) {
            from.files = repo.commitFiles(repo.resolveRev(pos[0]));
            to.files = repo.commitFiles(repo.resolveRev(pos[1]));
        } else if (pos.empty() && (hasFlag(args, "--staged") || hasFlag(args, "--cached"))) {
            from.files = repo.headFiles();
            to.files = repo.index().entries();
        } else if (pos.empty()) {
            from.files = repo.index().entries();
            to.work = true;
            const FileMap& idx = repo.index().entries();
            for (FileMap::const_iterator it = idx.begin(); it != idx.end(); ++it)
                if (fs::isFile(it->first)) to.files[it->first] = FileEntry("100644", repo.hashFile(it->first));
        } else {
            throw MiniGitError("usage: minigit diff | diff --staged | diff <rev> | diff <rev1> <rev2>");
        }
        printDiff(repo, from, to);
        return 0;
    }
};

// ================================================================= branch ====
class BranchCommand : public Command {
public:
    string name() const override { return "branch"; }
    string undoKind() const override { return "refs"; }
    int run(Repository& repo, const vector<string>& args) override {
        vector<string> pos = positionals(args, noValueFlags());
        Refs& refs = repo.refs();

        if (hasFlag(args, "-d") || hasFlag(args, "-D")) {
            if (pos.size() != 1) throw MiniGitError("usage: minigit branch -d <name>");
            const string& n = pos[0];
            if (!refs.branchExists(n)) throw MiniGitError("branch '" + n + "' not found");
            if (refs.currentBranch() == n) throw MiniGitError("cannot delete the branch you are on");
            if (hasFlag(args, "-d") && !repo.isAncestor(refs.branchTip(n), refs.headCommit()))
                throw MiniGitError("branch '" + n + "' is not fully merged (use -D to delete anyway)");
            string tip = refs.branchTip(n);
            refs.deleteBranch(n);
            cout << "Deleted branch " << n << " (was " << util::shortHash(tip) << ")\n";
            return 0;
        }
        if (pos.empty()) {
            vector<string> bs = refs.branches();
            string cur = refs.currentBranch();
            if (hasFlag(args, "--json")) {
                cout << "{\"current\":\"" << util::jsonEscape(cur) << "\",\"branches\":[";
                for (size_t i = 0; i < bs.size(); ++i)
                    cout << (i ? "," : "") << "{\"name\":\"" << util::jsonEscape(bs[i]) << "\",\"tip\":\""
                         << refs.branchTip(bs[i]) << "\"}";
                cout << "]}\n";
                return 0;
            }
            for (size_t i = 0; i < bs.size(); ++i) cout << (bs[i] == cur ? "* " : "  ") << bs[i] << "\n";
            return 0;
        }
        const string& n = pos[0];
        if (!Refs::validName(n)) throw MiniGitError("'" + n + "' is not a valid branch name");
        if (refs.branchExists(n)) throw MiniGitError("a branch named '" + n + "' already exists");
        string start = pos.size() > 1 ? repo.resolveRev(pos[1]) : refs.headCommit();
        if (start.empty()) throw MiniGitError("cannot create a branch: there are no commits yet");
        refs.setBranch(n, start);
        return 0;
    }
};

// =============================================================== checkout ====
class CheckoutCommand : public Command {
public:
    string name() const override { return "checkout"; }
    string undoKind() const override { return "tree"; }
    int run(Repository& repo, const vector<string>& args) override {
        vector<string> pos = positionals(args, noValueFlags());
        if (pos.size() != 1) throw MiniGitError("usage: minigit checkout <branch|rev>   or   minigit checkout -b <new-branch>");
        cout << repo.checkout(pos[0], hasFlag(args, "-b")) << "\n";
        return 0;
    }
};

// ================================================================== merge ====
class MergeCommand : public Command {
public:
    string name() const override { return "merge"; }
    string undoKind() const override { return "tree"; }
    int run(Repository& repo, const vector<string>& args) override {
        if (hasFlag(args, "--abort")) { repo.abortMerge(); cout << "Merge aborted.\n"; return 0; }
        vector<string> pos = positionals(args, noValueFlags());
        if (pos.size() != 1) throw MiniGitError("usage: minigit merge <branch>   or   minigit merge --abort");
        MergeResult r = repo.merge(pos[0]);
        for (size_t i = 0; i < r.conflicts.size(); ++i) cout << "CONFLICT (content): " << r.conflicts[i] << "\n";
        cout << r.message << "\n";
        return r.conflict ? 1 : 0;
    }
};

// ============================================================== undo / redo ==
class UndoCommand : public Command {
public:
    string name() const override { return "undo"; }
    int run(Repository& repo, const vector<string>&) override {
        cout << "Undid: " << repo.undo() << "\n";
        return 0;
    }
};

class RedoCommand : public Command {
public:
    string name() const override { return "redo"; }
    int run(Repository& repo, const vector<string>&) override {
        cout << "Redid: " << repo.redo() << "\n";
        return 0;
    }
};

// ================================================================= search ====
static vector<string> wordsOf(const string& text) {
    vector<string> out;
    string cur;
    for (size_t i = 0; i <= text.size(); ++i) {
        if (i < text.size() && std::isalnum((unsigned char)text[i])) cur += (char)std::tolower((unsigned char)text[i]);
        else if (!cur.empty()) { out.push_back(cur); cur.clear(); }
    }
    return out;
}

class SearchCommand : public Command {
public:
    string name() const override { return "search"; }
    int run(Repository& repo, const vector<string>& args) override {
        vector<string> terms;
        for (size_t i = 0; i < args.size(); ++i) {
            vector<string> w = wordsOf(args[i]);
            terms.insert(terms.end(), w.begin(), w.end());
        }
        if (terms.empty()) throw MiniGitError("usage: minigit search <word-or-prefix>...");

        vector<CommitInfo> all = repo.allHistory();
        Trie trie;
        for (size_t i = 0; i < all.size(); ++i) {
            vector<string> w = wordsOf(all[i].commit.message + " " + all[i].commit.authorName());
            for (size_t k = 0; k < w.size(); ++k) trie.insert(w[k], all[i].hash);
        }
        set<string> result = trie.withPrefix(terms[0]);
        for (size_t t = 1; t < terms.size(); ++t) {         // AND of all terms
            set<string> next = trie.withPrefix(terms[t]), both;
            std::set_intersection(result.begin(), result.end(), next.begin(), next.end(),
                                  std::inserter(both, both.begin()));
            result.swap(both);
        }
        int shown = 0;
        for (size_t i = 0; i < all.size(); ++i) {           // 'all' is already newest first
            if (!result.count(all[i].hash)) continue;
            cout << util::shortHash(all[i].hash) << " " << all[i].commit.summary() << "\n";
            ++shown;
        }
        if (!shown) cout << "no commits match\n";
        return 0;
    }
};

// ================================================================== stats ====
class StatsCommand : public Command {
public:
    string name() const override { return "stats"; }
    int run(Repository& repo, const vector<string>&) override {
        vector<CommitInfo> all = repo.allHistory();
        std::map<string, int> authors, changes;
        for (size_t i = 0; i < all.size(); ++i) {
            const Commit& c = all[i].commit;
            authors[c.authorName()]++;
            FileMap now = repo.commitFiles(all[i].hash);
            FileMap before = c.parents.empty() ? FileMap() : repo.commitFiles(c.parents[0]);
            for (FileMap::iterator it = now.begin(); it != now.end(); ++it) {
                FileMap::iterator b = before.find(it->first);
                if (b == before.end() || b->second.hash != it->second.hash) changes[it->first]++;
            }
            for (FileMap::iterator it = before.begin(); it != before.end(); ++it)
                if (!now.count(it->first)) changes[it->first]++;
        }
        cout << "Commits:       " << all.size() << "\n";
        cout << "Branches:      " << repo.refs().branches().size() << "\n";
        cout << "Objects:       " << repo.store().allHashes().size() << "\n";
        cout << "Contributors:  " << authors.size() << "\n";

        // Top contributors and most-changed files via a max-heap (priority queue).
        std::priority_queue<std::pair<int, string> > people;
        for (std::map<string, int>::iterator it = authors.begin(); it != authors.end(); ++it)
            people.push(std::make_pair(it->second, it->first));
        for (int k = 0; k < 5 && !people.empty(); ++k) {
            cout << "  " << people.top().second << " (" << people.top().first << " commits)\n";
            people.pop();
        }
        std::priority_queue<std::pair<int, string> > files;
        for (std::map<string, int>::iterator it = changes.begin(); it != changes.end(); ++it)
            files.push(std::make_pair(it->second, it->first));
        cout << "Most changed files:\n";
        for (int k = 0; k < 5 && !files.empty(); ++k) {
            cout << "  " << files.top().first << "x  " << files.top().second << "\n";
            files.pop();
        }
        return 0;
    }
};

// ================================================================= verify ====
class VerifyCommand : public Command {
public:
    string name() const override { return "verify"; }
    int run(Repository& repo, const vector<string>&) override {
        vector<string> problems;
        int n = repo.verify(problems);
        if (problems.empty()) { cout << "Checked " << n << " objects: everything is intact.\n"; return 0; }
        for (size_t i = 0; i < problems.size(); ++i) cout << "PROBLEM: " << problems[i] << "\n";
        cout << "Checked " << n << " objects: " << problems.size() << " problem(s) found.\n";
        return 1;
    }
};

// ================================================================ plumbing ===
class CatFileCommand : public Command {
public:
    string name() const override { return "cat-file"; }
    int run(Repository& repo, const vector<string>& args) override {
        vector<string> pos = positionals(args, noValueFlags());
        if (pos.size() != 1 || !(hasFlag(args, "-t") || hasFlag(args, "-s") || hasFlag(args, "-p")))
            throw MiniGitError("usage: minigit cat-file -t|-s|-p <hash-or-rev>");
        string hash = repo.store().resolvePrefix(pos[0]);
        if (hash.empty()) hash = repo.resolveRev(pos[0]);
        string type, body;
        repo.store().readRaw(hash, type, body);
        if (hasFlag(args, "-t")) { cout << type << "\n"; return 0; }
        if (hasFlag(args, "-s")) { cout << body.size() << "\n"; return 0; }
        if (type == "tree") {
            Tree t = Tree::parse(body);
            for (size_t i = 0; i < t.entries.size(); ++i) {
                const TreeEntry& e = t.entries[i];
                string mode = e.mode.size() < 6 ? string(6 - e.mode.size(), '0') + e.mode : e.mode;
                cout << mode << " " << (e.isDir() ? "tree" : "blob") << " " << e.hash << "\t" << e.name << "\n";
            }
        } else {
            cout << body;
        }
        return 0;
    }
};

class HashObjectCommand : public Command {
public:
    string name() const override { return "hash-object"; }
    int run(Repository& repo, const vector<string>& args) override {
        vector<string> pos = positionals(args, noValueFlags());
        if (pos.size() != 1) throw MiniGitError("usage: minigit hash-object [-w] <file>");
        string content = fs::readFile(repo.toRepoPath(pos[0]));
        cout << (hasFlag(args, "-w") ? repo.store().writeRaw("blob", content) : Object::hashOf("blob", content)) << "\n";
        return 0;
    }
};


// ===================================================================== ui ====
class UiCommand : public Command {
public:
    string name() const override { return "ui"; }
    bool needsRepo() const override { return false; }
    int run(Repository&, const vector<string>& args) override {
        set<string> vf; vf.insert("--port");
        vector<string> pos = positionals(args, vf);
        int port = (int)util::toInt(optValue(args, "--port"), 8080);
        if (!pos.empty()) {
            string folder = pos[0];
            for (size_t i = 0; i < folder.size(); ++i) if (folder[i] == '\\') folder[i] = '/';
            fs::makeDirs(folder);
            if (!fs::changeDir(folder)) throw MiniGitError("cannot open folder '" + pos[0] + "'");
        }
        return runServer(port, !hasFlag(args, "--no-browser"));
    }
};

// =================================================================== help ====
void printUsage() {
    cout <<
"minigit - a small Git-compatible version control system\n"
"\n"
"usage: minigit <command> [options]\n"
"\n"
"Setup\n"
"  init                        create an empty repository in this folder\n"
"  config <key> [value]        get/set user.name and user.email\n"
"\n"
"Everyday work\n"
"  add <path>...               stage files ('.' = everything)\n"
"  commit -m <message>         record the staged files\n"
"  status [--json]             show staged, modified and untracked files\n"
"  log [rev] [--oneline] [-n N] [--json]\n"
"  diff                        working folder vs staged\n"
"  diff --staged               staged vs last commit\n"
"  diff <rev>                  what one commit changed\n"
"  diff <rev1> <rev2>          between two commits\n"
"\n"
"Branches\n"
"  branch [name] [start]       list or create a branch   (--json to list as JSON)\n"
"  branch -d|-D <name>         delete a branch\n"
"  checkout [-b] <branch|rev>  switch branch / commit, or create a branch\n"
"  merge <branch>              merge a branch into the current one\n"
"  merge --abort               cancel a merge that has conflicts\n"
"\n"
"Extras\n"
"  undo | redo                 reverse / replay the last commit, branch, checkout or merge\n"
"  search <word>...            find commits by message/author words (prefix match, uses a trie)\n"
"  stats                       commit, author and file statistics\n"
"  verify                      check every object's integrity (SHA-1 + links)\n"
"\n"
"Graphical interface\n"
"  ui [folder] [--port N]      open the web interface in your browser (Ctrl+C to stop)\n"
"\n"
"Plumbing (same names as real Git)\n"
"  cat-file -t|-s|-p <rev>     show an object's type, size or content\n"
"  hash-object [-w] <file>     print (and optionally store) a file's blob hash\n"
"\n"
"Revisions can be HEAD, HEAD~2, a branch name, or a full/abbreviated hash.\n"
"Put patterns (like *.o or build/) in a .minigitignore file to skip them.\n";
}

// ============================================================== registry ====
CommandTable buildCommandTable() {
    CommandTable t;
    Command* all[] = {
        new InitCommand(), new ConfigCommand(), new AddCommand(), new CommitCommand(),
        new StatusCommand(), new LogCommand(), new DiffCommand(), new BranchCommand(),
        new CheckoutCommand(), new MergeCommand(), new UndoCommand(), new RedoCommand(),
        new SearchCommand(), new StatsCommand(), new VerifyCommand(), new CatFileCommand(),
        new HashObjectCommand(), new UiCommand()
    };
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); ++i)
        t[all[i]->name()] = std::shared_ptr<Command>(all[i]);
    return t;
}

// ======================================================= shared entry point ==
static bool sameState(const RepoState& a, const RepoState& b) {
    return a.head == b.head && a.branches == b.branches;
}

// Runs one command exactly like the command line does. Used by main() and by
// the web interface, so both behave identically (including undo recording).
int runCommand(const string& name, const vector<string>& args, bool searchUpward, std::ostream& err) {
    CommandTable table = buildCommandTable();
    CommandTable::iterator it = table.find(name);
    if (it == table.end()) {
        err << "minigit: '" << name << "' is not a minigit command. See 'minigit help'.\n";
        return 1;
    }
    Command& cmd = *it->second;
    try {
        string prefix;
        if (cmd.needsRepo()) {
            if (searchUpward) {
                if (!Repository::locate(prefix))
                    throw MiniGitError("not a minigit repository (run 'minigit init' first)");
            } else if (!fs::isDir(Repository::DIR)) {
                throw MiniGitError("not a minigit repository (run 'minigit init' first)");
            }
        }
        Repository repo(prefix);
        string kind = cmd.undoKind();
        RepoState before;
        if (!kind.empty()) before = repo.captureState();

        int rc = cmd.run(repo, args);

        // Record the operation for undo/redo only if it really moved something.
        if (rc == 0 && !kind.empty()) {
            RepoState after = repo.captureState();
            if (!sameState(before, after)) {
                string desc = name;
                for (size_t i = 0; i < args.size(); ++i) desc += " " + args[i];
                repo.recordOperation(kind, desc, before);
            }
        }
        return rc;
    } catch (const MiniGitError& e) {
        err << "fatal: " << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        err << "error: " << e.what() << "\n";
        return 2;
    }
}
