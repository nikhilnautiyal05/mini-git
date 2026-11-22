#include "Repository.h"
#include "FileUtil.h"
#include "Sha1.h"
#include <algorithm>
#include <queue>
#include <ctime>
#include <cstdio>
#include <cstdlib>

using std::string;
using std::vector;
using std::set;

const string Repository::DIR = ".minigit";
const string Repository::DEFAULT_BRANCH = "master";

// ============================================================ small helpers ==
static string tzString(time_t t) {
    struct tm lt = *localtime(&t);
    struct tm gt = *gmtime(&t);
    int diff = (lt.tm_hour - gt.tm_hour) * 60 + (lt.tm_min - gt.tm_min);
    if (lt.tm_year != gt.tm_year) diff += (lt.tm_year > gt.tm_year ? 1 : -1) * 1440;
    else diff += (lt.tm_yday - gt.tm_yday) * 1440;
    char sign = diff < 0 ? '-' : '+';
    if (diff < 0) diff = -diff;
    char buf[16];
    std::sprintf(buf, "%c%02d%02d", sign, diff / 60, diff % 60);
    return buf;
}

static bool sameEntry(const FileEntry* a, const FileEntry* b) {
    if (!a && !b) return true;
    if (!a || !b) return false;
    return a->hash == b->hash;
}

static string withNewline(const string& s) {
    if (s.empty() || s[s.size() - 1] == '\n') return s;
    return s + "\n";
}

// ============================================================ locating repo ==
bool Repository::locate(string& prefix) {
    string original = fs::cwd();
    string rel;
    for (int depth = 0; depth < 100; ++depth) {
        if (fs::isDir(DIR)) { prefix = rel; return true; }
        string here = fs::cwd();
        if (!fs::changeDir("..") || fs::cwd() == here) break;
        size_t slash = here.find_last_of('/');
        string name = slash == string::npos ? here : here.substr(slash + 1);
        rel = rel.empty() ? name : name + "/" + rel;
    }
    fs::changeDir(original);
    return false;
}

Repository::Repository(const string& prefix)
    : prefix_(prefix), store_(DIR + "/objects"), refs_(DIR), index_(DIR + "/index") {
    if (fs::isDir(DIR)) { index_.load(); loadIgnore(); }
}

string Repository::toRepoPath(const string& userPath) const {
    string p = fs::normalize(prefix_.empty() ? userPath : prefix_ + "/" + userPath);
    if (p == ".." || util::startsWith(p, "../"))
        throw MiniGitError("path '" + userPath + "' is outside the repository");
    return p;
}

bool Repository::init() {
    bool existed = fs::isDir(DIR);
    fs::makeDirs(DIR + "/objects");
    fs::makeDirs(DIR + "/refs/heads");
    if (!fs::exists(DIR + "/HEAD")) fs::writeFile(DIR + "/HEAD", "ref: refs/heads/" + DEFAULT_BRANCH + "\n");
    if (!fs::exists(DIR + "/index")) index_.save();          // valid empty index
    if (!fs::exists(DIR + "/config"))
        fs::writeFile(DIR + "/config", "[core]\n\trepositoryformatversion = 0\n\tfilemode = false\n\tbare = false\n");
    return existed;
}

// ================================================================== config ==
// Config uses Git's own INI layout so real Git can parse the file:
//   [user]
//       name = Alice
static std::map<string, string> loadConfig() {
    std::map<string, string> kv;
    if (!fs::isFile(Repository::DIR + "/config")) return kv;
    vector<string> lines = util::splitLines(fs::readFile(Repository::DIR + "/config"));
    string section;
    for (size_t i = 0; i < lines.size(); ++i) {
        string l = util::trim(lines[i]);
        if (l.empty() || l[0] == '#' || l[0] == ';') continue;
        if (l[0] == '[') {
            size_t close = l.find(']');
            section = util::lower(l.substr(1, close == string::npos ? string::npos : close - 1));
            continue;
        }
        size_t eq = l.find('=');
        if (eq == string::npos || section.empty()) continue;
        kv[section + "." + util::lower(util::trim(l.substr(0, eq)))] = util::trim(l.substr(eq + 1));
    }
    return kv;
}

string Repository::getConfig(const string& key) const {
    std::map<string, string> kv = loadConfig();
    std::map<string, string>::iterator it = kv.find(util::lower(key));
    return it == kv.end() ? "" : it->second;
}

void Repository::setConfig(const string& key, const string& value) {
    size_t dot = key.find('.');
    if (dot == string::npos || dot == 0 || dot + 1 >= key.size())
        throw MiniGitError("key must look like section.name (for example user.name)");
    std::map<string, string> kv = loadConfig();
    kv[util::lower(key)] = value;
    string out, current;
    for (std::map<string, string>::iterator it = kv.begin(); it != kv.end(); ++it) {
        size_t d = it->first.find('.');
        string sec = it->first.substr(0, d);
        if (sec != current) { out += "[" + sec + "]\n"; current = sec; }
        out += "\t" + it->first.substr(d + 1) + " = " + it->second + "\n";
    }
    fs::writeFile(DIR + "/config", out);
}

string Repository::identity() const {
    string name = getConfig("user.name"), email = getConfig("user.email");
    if (name.empty()) {
        const char* e = std::getenv("USERNAME");
        if (!e) e = std::getenv("USER");
        name = e ? e : "Unknown";
    }
    if (email.empty()) email = "unknown@localhost";
    time_t now = time(0);
    return name + " <" + email + "> " + util::toStr((long long)now) + " " + tzString(now);
}

// ============================================================ working folder ==
void Repository::loadIgnore() {
    ignore_.clear();
    if (!fs::isFile(".minigitignore")) return;
    vector<string> lines = util::splitLines(fs::readFile(".minigitignore"));
    for (size_t i = 0; i < lines.size(); ++i) {
        string l = util::trim(lines[i]);
        if (!l.empty() && l[0] != '#') ignore_.push_back(l);
    }
}

bool Repository::isIgnored(const string& rel) const {
    vector<string> parts = util::split(rel, '/');
    for (size_t i = 0; i < parts.size(); ++i)
        if (parts[i] == ".minigit" || parts[i] == ".git") return true;
    for (size_t i = 0; i < ignore_.size(); ++i) {
        string p = ignore_[i];
        if (util::endsWith(p, "/")) p = p.substr(0, p.size() - 1);
        if (p.empty()) continue;
        if (p == rel) return true;
        if (p[0] == '*') {
            if (util::endsWith(parts.back(), p.substr(1))) return true;
        } else {
            for (size_t j = 0; j < parts.size(); ++j) if (parts[j] == p) return true;
        }
    }
    return false;
}

void Repository::walk(const string& dir, vector<string>& out) const {
    vector<string> names = fs::listDir(dir.empty() ? "." : dir);
    for (size_t i = 0; i < names.size(); ++i) {
        string rel = dir.empty() ? names[i] : dir + "/" + names[i];
        if (isIgnored(rel)) continue;
        if (fs::isDir(rel)) walk(rel, out);
        else out.push_back(rel);
    }
}

vector<string> Repository::workingFiles() {
    vector<string> out;
    walk("", out);
    return out;
}

string Repository::hashFile(const string& path) {
    return Object::hashOf("blob", fs::readFile(path));
}

// ====================================================================== add ==
void Repository::stageFile(const string& path) {
    string hash = store_.writeRaw("blob", fs::readFile(path));
    index_.entries()[path] = FileEntry("100644", hash);
}

void Repository::add(const vector<string>& paths) {
    FileMap& idx = index_.entries();
    for (size_t k = 0; k < paths.size(); ++k) {
        const string& p = paths[k];
        if (p.empty() || fs::isDir(p)) {
            string prefix = p.empty() ? "" : p + "/";
            vector<string> files = workingFiles();
            for (size_t i = 0; i < files.size(); ++i)
                if (util::startsWith(files[i], prefix)) stageFile(files[i]);
            // files that disappeared from the folder get removed from the index
            for (FileMap::iterator it = idx.begin(); it != idx.end();) {
                if (util::startsWith(it->first, prefix) && !fs::isFile(it->first)) idx.erase(it++);
                else ++it;
            }
        } else if (fs::isFile(p)) {
            if (isIgnored(p)) throw MiniGitError("'" + p + "' is ignored (see .minigitignore)");
            stageFile(p);
        } else {
            bool removed = false;
            for (FileMap::iterator it = idx.begin(); it != idx.end();) {
                if (it->first == p || util::startsWith(it->first, p + "/")) { idx.erase(it++); removed = true; }
                else ++it;
            }
            if (!removed) throw MiniGitError("pathspec '" + p + "' did not match any files");
        }
    }
    index_.save();
}

// ==================================================================== trees ==
// Build the tree objects for a flat path->blob map. Folders become nested trees.
string Repository::writeTree(const FileMap& files) {
    Tree tree;
    std::map<string, FileMap> subdirs;
    for (FileMap::const_iterator it = files.begin(); it != files.end(); ++it) {
        size_t slash = it->first.find('/');
        if (slash == string::npos) {
            TreeEntry e;
            e.mode = it->second.mode;
            e.name = it->first;
            e.hash = it->second.hash;
            tree.entries.push_back(e);
        } else {
            subdirs[it->first.substr(0, slash)][it->first.substr(slash + 1)] = it->second;
        }
    }
    for (std::map<string, FileMap>::iterator it = subdirs.begin(); it != subdirs.end(); ++it) {
        TreeEntry e;
        e.mode = "40000";
        e.name = it->first;
        e.hash = writeTree(it->second);
        tree.entries.push_back(e);
    }
    return store_.write(tree);
}

void Repository::flattenTree(const string& treeHash, const string& prefix, FileMap& out) {
    Tree t = store_.loadTree(treeHash);
    for (size_t i = 0; i < t.entries.size(); ++i) {
        const TreeEntry& e = t.entries[i];
        if (e.isDir()) flattenTree(e.hash, prefix + e.name + "/", out);
        else out[prefix + e.name] = FileEntry(e.mode, e.hash);
    }
}

FileMap Repository::commitFiles(const string& commitHash) {
    FileMap out;
    if (commitHash.empty()) return out;
    flattenTree(getCommit(commitHash).tree, "", out);
    return out;
}

// ================================================================== commits ==
const Commit& Repository::getCommit(const string& hash) {
    std::map<string, Commit>::iterator it = commitCache_.find(hash);
    if (it == commitCache_.end()) it = commitCache_.insert(std::make_pair(hash, store_.loadCommit(hash))).first;
    return it->second;
}

string Repository::createCommit(const string& tree, const vector<string>& parents, const string& message) {
    Commit c;
    c.tree = tree;
    c.parents = parents;
    c.author = c.committer = identity();
    c.message = message;
    return store_.write(c);
}

bool Repository::merging() const { return fs::exists(DIR + "/MERGE_HEAD"); }

string Repository::mergeMessage() const {
    return fs::isFile(DIR + "/MERGE_MSG") ? util::trim(fs::readFile(DIR + "/MERGE_MSG")) : "";
}

string Repository::commit(const string& rawMessage) {
    string message = util::rtrim(rawMessage);
    if (message.empty()) throw MiniGitError("empty commit message");
    message += "\n";

    const FileMap& idx = index_.entries();
    if (idx.empty()) throw MiniGitError("nothing to commit (the staging area is empty; use 'minigit add')");

    string parent = refs_.headCommit();
    bool isMerge = merging();
    string treeHash = writeTree(idx);
    if (!parent.empty() && !isMerge && getCommit(parent).tree == treeHash)
        throw MiniGitError("nothing to commit (no staged changes)");

    vector<string> parents;
    if (!parent.empty()) parents.push_back(parent);
    if (isMerge) parents.push_back(util::trim(fs::readFile(DIR + "/MERGE_HEAD")));

    string hash = createCommit(treeHash, parents, message);
    refs_.updateHead(hash);
    if (isMerge) {
        fs::removeFile(DIR + "/MERGE_HEAD");
        fs::removeFile(DIR + "/MERGE_MSG");
    }
    return hash;
}

// ===================================================================== log ===
// Newest-first ordering that is also topologically correct: a commit is only
// listed after every commit that has it as a parent. A max-heap on the commit
// time picks the next one, so even same-second commits and merges (a DAG)
// come out in the right order.
vector<CommitInfo> Repository::walkOrdered(const vector<string>& starts, int limit) {
    std::map<string, int> pendingChildren;
    set<string> seen;
    vector<string> stack;
    for (size_t i = 0; i < starts.size(); ++i) if (!starts[i].empty()) stack.push_back(starts[i]);
    while (!stack.empty()) {
        string h = stack.back();
        stack.pop_back();
        if (!seen.insert(h).second) continue;
        pendingChildren[h];                                   // make sure the entry exists
        const Commit& c = getCommit(h);
        for (size_t i = 0; i < c.parents.size(); ++i) {
            pendingChildren[c.parents[i]]++;
            stack.push_back(c.parents[i]);
        }
    }
    typedef std::pair<long long, string> Item;
    std::priority_queue<Item> heap;
    for (std::map<string, int>::iterator it = pendingChildren.begin(); it != pendingChildren.end(); ++it)
        if (it->second == 0) heap.push(Item(getCommit(it->first).timestamp(), it->first));

    vector<CommitInfo> out;
    while (!heap.empty() && (limit < 0 || (int)out.size() < limit)) {
        string h = heap.top().second;
        heap.pop();
        CommitInfo ci;
        ci.hash = h;
        ci.commit = getCommit(h);
        out.push_back(ci);
        for (size_t i = 0; i < ci.commit.parents.size(); ++i) {
            const string& p = ci.commit.parents[i];
            if (--pendingChildren[p] == 0) heap.push(Item(getCommit(p).timestamp(), p));
        }
    }
    return out;
}

vector<CommitInfo> Repository::history(const string& start, int limit) {
    return walkOrdered(vector<string>(1, start), limit);
}

vector<CommitInfo> Repository::allHistory() {
    vector<string> tips;
    vector<string> bs = refs_.branches();
    for (size_t i = 0; i < bs.size(); ++i) tips.push_back(refs_.branchTip(bs[i]));
    tips.push_back(refs_.headCommit());
    return walkOrdered(tips, -1);
}

// Accepts: HEAD, HEAD~N, <branch>, <branch>~N, full hash, abbreviated hash.
string Repository::resolveRev(const string& rev) {
    string base = rev;
    int steps = 0;
    size_t tilde = rev.find('~');
    if (tilde != string::npos) {
        base = rev.substr(0, tilde);
        string n = rev.substr(tilde + 1);
        steps = n.empty() ? 1 : (int)util::toInt(n, -1);
        if (steps < 0) throw MiniGitError("bad revision '" + rev + "'");
    }
    string hash;
    if (base == "HEAD") {
        hash = refs_.headCommit();
        if (hash.empty()) throw MiniGitError("HEAD does not point to a commit yet");
    } else if (refs_.branchExists(base)) {
        hash = refs_.branchTip(base);
    } else {
        hash = store_.resolvePrefix(base);
    }
    if (hash.empty()) throw MiniGitError("unknown revision '" + rev + "'");
    for (int i = 0; i < steps; ++i) {
        const Commit& c = getCommit(hash);
        if (c.parents.empty()) throw MiniGitError("'" + rev + "' goes past the first commit");
        hash = c.parents[0];
    }
    getCommit(hash);   // make sure it really is a commit
    return hash;
}

bool Repository::isAncestor(const string& anc, const string& of) {
    std::vector<string> stack(1, of);
    set<string> seen;
    seen.insert(of);
    while (!stack.empty()) {
        string h = stack.back();
        stack.pop_back();
        if (h == anc) return true;
        const Commit& c = getCommit(h);
        for (size_t i = 0; i < c.parents.size(); ++i)
            if (seen.insert(c.parents[i]).second) stack.push_back(c.parents[i]);
    }
    return false;
}

// Nearest common ancestor: collect everything reachable from 'a', then do a
// breadth-first search from 'b' until it touches that set.
string Repository::mergeBase(const string& a, const string& b) {
    set<string> ancestors;
    std::vector<string> stack(1, a);
    while (!stack.empty()) {
        string h = stack.back();
        stack.pop_back();
        if (!ancestors.insert(h).second) continue;
        const Commit& c = getCommit(h);
        for (size_t i = 0; i < c.parents.size(); ++i) stack.push_back(c.parents[i]);
    }
    std::queue<string> q;
    set<string> seen;
    q.push(b);
    seen.insert(b);
    while (!q.empty()) {
        string h = q.front();
        q.pop();
        if (ancestors.count(h)) return h;
        const Commit& c = getCommit(h);
        for (size_t i = 0; i < c.parents.size(); ++i)
            if (seen.insert(c.parents[i]).second) q.push(c.parents[i]);
    }
    return "";
}

// =================================================================== status ==
StatusResult Repository::status() {
    StatusResult st;
    st.merging = merging();
    FileMap head = headFiles();
    const FileMap& idx = index_.entries();

    for (FileMap::const_iterator it = idx.begin(); it != idx.end(); ++it) {
        FileMap::const_iterator h = head.find(it->first);
        if (h == head.end()) st.staged.push_back(std::make_pair('A', it->first));
        else if (h->second.hash != it->second.hash) st.staged.push_back(std::make_pair('M', it->first));
    }
    for (FileMap::const_iterator it = head.begin(); it != head.end(); ++it)
        if (!idx.count(it->first)) st.staged.push_back(std::make_pair('D', it->first));
    std::sort(st.staged.begin(), st.staged.end(),
              [](const std::pair<char, string>& a, const std::pair<char, string>& b) { return a.second < b.second; });

    for (FileMap::const_iterator it = idx.begin(); it != idx.end(); ++it) {
        if (!fs::isFile(it->first)) st.unstaged.push_back(std::make_pair('D', it->first));
        else if (hashFile(it->first) != it->second.hash) st.unstaged.push_back(std::make_pair('M', it->first));
    }

    vector<string> work = workingFiles();
    for (size_t i = 0; i < work.size(); ++i)
        if (!idx.count(work[i])) st.untracked.push_back(work[i]);
    return st;
}

// ================================================ switching versions of files ==
void Repository::ensureClean() {
    StatusResult st = status();
    if (!st.clean())
        throw MiniGitError("you have uncommitted changes that would be lost; commit them first");
}

// Make the working folder and index match 'toCommit' (starting from 'fromCommit').
void Repository::applyTree(const string& fromCommit, const string& toCommit) {
    FileMap cur = commitFiles(fromCommit), target = commitFiles(toCommit);

    for (FileMap::iterator it = target.begin(); it != target.end(); ++it)
        if (!cur.count(it->first) && fs::exists(it->first))
            throw MiniGitError("untracked file '" + it->first + "' would be overwritten; move or delete it first");

    for (FileMap::iterator it = cur.begin(); it != cur.end(); ++it) {
        if (!target.count(it->first)) {
            fs::removeFile(it->first);
            fs::removeEmptyParents(it->first);
        }
    }
    for (FileMap::iterator it = target.begin(); it != target.end(); ++it) {
        FileMap::iterator c = cur.find(it->first);
        if (c == cur.end() || c->second.hash != it->second.hash || !fs::isFile(it->first))
            fs::writeFile(it->first, store_.blobContent(it->second.hash));
    }
    index_.entries() = target;
    index_.save();
}

void Repository::switchTree(const string& fromCommit, const string& toCommit) {
    ensureClean();
    applyTree(fromCommit, toCommit);
}

void Repository::resetHard() {
    FileMap target = headFiles();
    FileMap& idx = index_.entries();
    for (FileMap::iterator it = idx.begin(); it != idx.end(); ++it)
        if (!target.count(it->first)) { fs::removeFile(it->first); fs::removeEmptyParents(it->first); }
    for (FileMap::iterator it = target.begin(); it != target.end(); ++it)
        fs::writeFile(it->first, store_.blobContent(it->second.hash));
    idx = target;
    index_.save();
    fs::removeFile(DIR + "/MERGE_HEAD");
    fs::removeFile(DIR + "/MERGE_MSG");
}

string Repository::checkout(const string& target, bool create) {
    if (create) {
        if (!Refs::validName(target)) throw MiniGitError("'" + target + "' is not a valid branch name");
        if (refs_.branchExists(target)) throw MiniGitError("a branch named '" + target + "' already exists");
        string head = refs_.headCommit();
        if (!head.empty()) refs_.setBranch(target, head);
        refs_.setHeadToBranch(target);
        return "Switched to a new branch '" + target + "'";
    }
    string current = refs_.headCommit();
    if (refs_.branchExists(target)) {
        if (refs_.currentBranch() == target) return "Already on '" + target + "'";
        switchTree(current, refs_.branchTip(target));
        refs_.setHeadToBranch(target);
        return "Switched to branch '" + target + "'";
    }
    string dest = resolveRev(target);
    switchTree(current, dest);
    refs_.setHeadDetached(dest);
    return "HEAD is now at " + util::shortHash(dest) + " " + getCommit(dest).summary() + " (detached HEAD)";
}

// ==================================================================== merge ==
MergeResult Repository::merge(const string& name) {
    MergeResult res;
    if (merging()) throw MiniGitError("a merge is already in progress: commit the result or run 'merge --abort'");
    string ours = refs_.headCommit();
    if (ours.empty()) throw MiniGitError("cannot merge: HEAD has no commits yet");
    string theirs = refs_.branchExists(name) ? refs_.branchTip(name) : resolveRev(name);
    ensureClean();

    if (theirs == ours || isAncestor(theirs, ours)) { res.message = "Already up to date."; return res; }
    if (isAncestor(ours, theirs)) {                          // fast-forward
        applyTree(ours, theirs);
        refs_.updateHead(theirs);
        res.message = "Fast-forward";
        return res;
    }

    string base = mergeBase(ours, theirs);
    if (base.empty()) throw MiniGitError("refusing to merge unrelated histories");
    FileMap B = commitFiles(base), O = commitFiles(ours), T = commitFiles(theirs);

    set<string> paths;
    for (FileMap::iterator it = B.begin(); it != B.end(); ++it) paths.insert(it->first);
    for (FileMap::iterator it = O.begin(); it != O.end(); ++it) paths.insert(it->first);
    for (FileMap::iterator it = T.begin(); it != T.end(); ++it) paths.insert(it->first);

    for (set<string>::iterator it = paths.begin(); it != paths.end(); ++it)
        if (!O.count(*it) && fs::exists(*it))
            throw MiniGitError("untracked file '" + *it + "' would be overwritten by the merge");

    FileMap merged;
    for (set<string>::iterator it = paths.begin(); it != paths.end(); ++it) {
        const string& p = *it;
        FileMap::iterator bi = B.find(p), oi = O.find(p), ti = T.find(p);
        const FileEntry* b = bi == B.end() ? 0 : &bi->second;
        const FileEntry* o = oi == O.end() ? 0 : &oi->second;
        const FileEntry* t = ti == T.end() ? 0 : &ti->second;

        // Three-way rule: if only one side changed a file, take that side.
        const FileEntry* pick = 0;
        bool conflict = false;
        if (sameEntry(o, t)) pick = o;
        else if (sameEntry(b, o)) pick = t;
        else if (sameEntry(b, t)) pick = o;
        else conflict = true;

        if (conflict) {
            string ourText = o ? store_.blobContent(o->hash) : "";
            string theirText = t ? store_.blobContent(t->hash) : "";
            fs::writeFile(p, "<<<<<<< HEAD\n" + withNewline(ourText) + "=======\n" +
                             withNewline(theirText) + ">>>>>>> " + name + "\n");
            if (o) merged[p] = *o;
            res.conflicts.push_back(p);
        } else if (pick) {
            merged[p] = *pick;
            if (!o || o->hash != pick->hash) fs::writeFile(p, store_.blobContent(pick->hash));
        } else if (o) {
            fs::removeFile(p);
            fs::removeEmptyParents(p);
        }
    }
    index_.entries() = merged;
    index_.save();

    if (!res.conflicts.empty()) {
        fs::writeFile(DIR + "/MERGE_HEAD", theirs + "\n");
        fs::writeFile(DIR + "/MERGE_MSG", "Merge branch '" + name + "'\n");
        res.conflict = true;
        res.message = "Automatic merge failed; fix the conflicts, 'minigit add' the files, then 'minigit commit'.";
        return res;
    }
    vector<string> parents;
    parents.push_back(ours);
    parents.push_back(theirs);
    string c = createCommit(writeTree(merged), parents, "Merge branch '" + name + "'\n");
    refs_.updateHead(c);
    res.message = "Merge made by the three-way strategy.";
    return res;
}

void Repository::abortMerge() {
    if (!merging()) throw MiniGitError("there is no merge to abort");
    resetHard();
}

// ============================================================== undo / redo ==
RepoState Repository::captureState() const {
    RepoState s;
    s.head = refs_.readHead();
    vector<string> bs = refs_.branches();
    for (size_t i = 0; i < bs.size(); ++i) s.branches[bs[i]] = refs_.branchTip(bs[i]);
    return s;
}

void Repository::restoreRefs(const RepoState& st) {
    vector<string> current = refs_.branches();
    for (size_t i = 0; i < current.size(); ++i)
        if (!st.branches.count(current[i])) refs_.deleteBranch(current[i]);
    for (std::map<string, string>::const_iterator it = st.branches.begin(); it != st.branches.end(); ++it)
        refs_.setBranch(it->first, it->second);
    refs_.writeHeadRaw(st.head);
}

void Repository::applyState(const RepoState& st, const string& kind) {
    if (kind == "tree") ensureClean();
    string oldCommit = refs_.headCommit();
    restoreRefs(st);
    if (kind == "tree") applyTree(oldCommit, refs_.headCommit());
}

void Repository::recordOperation(const string& kind, const string& desc, const RepoState& before) {
    UndoEntry e;
    e.kind = kind;
    e.desc = desc;
    for (size_t i = 0; i < e.desc.size(); ++i)
        if (e.desc[i] == '\t' || e.desc[i] == '\n' || e.desc[i] == '\r') e.desc[i] = ' ';
    if (e.desc.size() > 60) e.desc = e.desc.substr(0, 57) + "...";
    e.state = before;

    vector<UndoEntry> stack = UndoLog::load(DIR + "/undo");
    stack.push_back(e);
    if (stack.size() > 50) stack.erase(stack.begin());
    UndoLog::save(DIR + "/undo", stack);
    UndoLog::save(DIR + "/redo", vector<UndoEntry>());     // a new action clears redo
}

string Repository::undo() {
    vector<UndoEntry> undoStack = UndoLog::load(DIR + "/undo");
    if (undoStack.empty()) throw MiniGitError("nothing to undo");
    UndoEntry e = undoStack.back();
    undoStack.pop_back();

    UndoEntry forRedo = e;
    forRedo.state = captureState();
    applyState(e.state, e.kind);                          // may throw: stacks stay untouched

    vector<UndoEntry> redoStack = UndoLog::load(DIR + "/redo");
    redoStack.push_back(forRedo);
    UndoLog::save(DIR + "/undo", undoStack);
    UndoLog::save(DIR + "/redo", redoStack);
    return e.desc;
}

string Repository::redo() {
    vector<UndoEntry> redoStack = UndoLog::load(DIR + "/redo");
    if (redoStack.empty()) throw MiniGitError("nothing to redo");
    UndoEntry e = redoStack.back();
    redoStack.pop_back();

    UndoEntry forUndo = e;
    forUndo.state = captureState();
    applyState(e.state, e.kind);

    vector<UndoEntry> undoStack = UndoLog::load(DIR + "/undo");
    undoStack.push_back(forUndo);
    UndoLog::save(DIR + "/undo", undoStack);
    UndoLog::save(DIR + "/redo", redoStack);
    return e.desc;
}

// ================================================================== verify ===
// Merkle-style integrity check: every object must hash to its own name, and
// every commit/tree must point only to objects that exist.
int Repository::verify(vector<string>& problems) {
    vector<string> all = store_.allHashes();
    set<string> present(all.begin(), all.end());
    for (size_t i = 0; i < all.size(); ++i) {
        const string& h = all[i];
        try {
            string type, body;
            store_.readRaw(h, type, body);
            string actual = Object::hashOf(type, body);
            if (actual != h) {
                problems.push_back("object " + h + " is corrupted (content hashes to " + actual + ")");
            } else if (type == "commit") {
                Commit c = Commit::parse(body);
                if (!present.count(c.tree)) problems.push_back("commit " + h + " is missing its tree " + c.tree);
                for (size_t k = 0; k < c.parents.size(); ++k)
                    if (!present.count(c.parents[k]))
                        problems.push_back("commit " + h + " is missing parent " + c.parents[k]);
            } else if (type == "tree") {
                Tree t = Tree::parse(body);
                for (size_t k = 0; k < t.entries.size(); ++k)
                    if (t.entries[k].mode != "160000" && !present.count(t.entries[k].hash))
                        problems.push_back("tree " + h + " is missing '" + t.entries[k].name + "' (" + t.entries[k].hash + ")");
            }
        } catch (const MiniGitError& e) {
            problems.push_back(h + ": " + e.what());
        }
    }
    vector<string> bs = refs_.branches();
    for (size_t i = 0; i < bs.size(); ++i)
        if (!present.count(refs_.branchTip(bs[i])))
            problems.push_back("branch '" + bs[i] + "' points to a missing commit");
    return (int)all.size();
}
