#include "Server.h"
#include "UiPage.h"
#include "Commands.h"
#include "Repository.h"
#include "FileUtil.h"
#include "Undo.h"
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <cctype>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #define NOMINMAX
  #include <winsock2.h>
  typedef SOCKET sock_t;
  typedef int mg_socklen;
  #define MG_CLOSE closesocket
  #define MG_BAD_SOCK INVALID_SOCKET
  #define MG_SEND_FLAGS 0
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <sys/time.h>
  #include <unistd.h>
  typedef int sock_t;
  typedef socklen_t mg_socklen;
  #define MG_CLOSE close
  #define MG_BAD_SOCK (-1)
  #ifdef MSG_NOSIGNAL
    #define MG_SEND_FLAGS MSG_NOSIGNAL
  #else
    #define MG_SEND_FLAGS 0
  #endif
#endif

using std::string;
using std::vector;

namespace {

// ------------------------------------------------------------------ HTTP ----
struct Request {
    string method, path, query, body;
    std::map<string, string> headers;     // names are lower-case
};

string jstr(const string& s) { return "\"" + util::jsonEscape(s) + "\""; }

string urlDecode(const string& s) {
    string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size() + 0 && std::isxdigit((unsigned char)s[i + 1]) &&
            std::isxdigit((unsigned char)s[i + 2])) {
            out += (char)std::strtol(s.substr(i + 1, 2).c_str(), 0, 16);
            i += 2;
        } else if (s[i] == '+') {
            out += ' ';
        } else {
            out += s[i];
        }
    }
    return out;
}

string queryParam(const string& query, const string& key) {
    vector<string> parts = util::split(query, '&');
    for (size_t i = 0; i < parts.size(); ++i) {
        size_t eq = parts[i].find('=');
        if (eq != string::npos && parts[i].substr(0, eq) == key) return urlDecode(parts[i].substr(eq + 1));
    }
    return "";
}

bool readRequest(sock_t c, Request& r) {
    string data;
    char buf[8192];
    size_t end;
    while ((end = data.find("\r\n\r\n")) == string::npos) {
        int n = recv(c, buf, sizeof(buf), 0);
        if (n <= 0) return false;
        data.append(buf, (size_t)n);
        if (data.size() > 8u * 1024 * 1024) return false;
    }
    r.body = data.substr(end + 4);
    vector<string> lines = util::split(data.substr(0, end), '\n');
    vector<string> first = util::split(util::trim(lines[0]), ' ');
    if (first.size() < 2) return false;
    r.method = first[0];
    string target = first[1];
    size_t q = target.find('?');
    r.path = q == string::npos ? target : target.substr(0, q);
    r.query = q == string::npos ? "" : target.substr(q + 1);
    for (size_t i = 1; i < lines.size(); ++i) {
        size_t colon = lines[i].find(':');
        if (colon == string::npos) continue;
        r.headers[util::lower(util::trim(lines[i].substr(0, colon)))] = util::trim(lines[i].substr(colon + 1));
    }
    long len = util::toInt(r.headers["content-length"], 0);
    if (len < 0 || len > 5L * 1024 * 1024) return false;
    while ((long)r.body.size() < len) {
        int n = recv(c, buf, sizeof(buf), 0);
        if (n <= 0) return false;
        r.body.append(buf, (size_t)n);
    }
    r.body.resize((size_t)len);
    return true;
}

void sendAll(sock_t c, const string& s) {
    size_t off = 0;
    while (off < s.size()) {
        int n = send(c, s.data() + off, (int)(s.size() - off), MG_SEND_FLAGS);
        if (n <= 0) return;
        off += (size_t)n;
    }
}

void respond(sock_t c, int code, const string& type, const string& body) {
    const char* text = code == 200 ? "OK" : code == 400 ? "Bad Request" : code == 403 ? "Forbidden"
                     : code == 404 ? "Not Found" : "Internal Server Error";
    std::ostringstream h;
    h << "HTTP/1.1 " << code << " " << text << "\r\nContent-Type: " << type << "; charset=utf-8\r\n"
      << "Content-Length: " << body.size() << "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";
    sendAll(c, h.str() + body);
}

void respondError(sock_t c, int code, const string& msg) {
    respond(c, code, "application/json", "{\"error\":" + jstr(msg) + "}");
}

// --------------------------------------------------------------- API logic ---
string safePath(const string& raw) { return fs::safeRelativePath(raw); }

// The GUI is a repository manager: each repository lives in
// <launch-folder>/Repositories/<name>. Requests are handled serially, so the
// process working directory can safely follow the selected repository.
string gRootDir;
string gSelectedRepo;

bool validRepoName(const string& name) {
    if (name.empty() || name == "." || name == "..") return false;
    for (size_t i = 0; i < name.size(); ++i) {
        unsigned char c = (unsigned char)name[i];
        if (!(std::isalnum(c) || c == '_' || c == '-' || c == ' ')) return false;
    }
    return true;
}

string repositoriesJson() {
    vector<string> names = fs::listDir(gRootDir + "/Repositories");
    std::ostringstream o;
    o << "[";
    bool first = true;
    for (size_t i = 0; i < names.size(); ++i) {
        if (!validRepoName(names[i]) || !fs::isDir(gRootDir + "/Repositories/" + names[i])) continue;
        o << (first ? "" : ",") << jstr(names[i]);
        first = false;
    }
    o << "]";
    return o.str();
}

// ------------------------------------------------------- page files (ui/) ---
// Folder of the running program (so "ui/" next to minigit.exe can be found).
string exeDir() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return "";
    string p(buf, n);
#elif defined(__linux__)
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return "";
    string p(buf, (size_t)n);
#else
    return "";
#endif
    for (size_t i = 0; i < p.size(); ++i) if (p[i] == '\\') p[i] = '/';
    size_t slash = p.find_last_of('/');
    return slash == string::npos ? "" : p.substr(0, slash);
}

// Serves index.html, editor.html, style.css, app.js, editor.js: from the ui folder if it exists
// (edit and press F5), otherwise from the copies built into the program.
void serveStatic(sock_t c, const string& path) {
    string name = path == "/" ? "index.html" : path == "/editor" ? "editor.html" : path.substr(1);
    const char* type = "text/html";
    const char* embedded = UI_HTML;
    if (name == "style.css")        { type = "text/css";               embedded = UI_CSS; }
    else if (name == "app.js")      { type = "application/javascript"; embedded = UI_JS; }
    else if (name == "editor.html") { embedded = UI_EDITOR_HTML; }
    else if (name == "editor.js")   { type = "application/javascript"; embedded = UI_EDITOR_JS; }

    string dir = exeDir();
    string file = dir.empty() ? "" : dir + "/ui/" + name;
    if (!file.empty() && fs::isFile(file)) respond(c, 200, type, fs::readFile(file));
    else respond(c, 200, type, embedded);
}

string stateJson() {
    std::ostringstream o;
    o << "{\"cwd\":" << jstr(fs::cwd())
      << ",\"selectedRepo\":" << jstr(gSelectedRepo)
      << ",\"repositories\":" << repositoriesJson();
    if (gSelectedRepo.empty() || !fs::isDir(Repository::DIR)) {
        o << ",\"initialized\":false,\"files\":[]}";
        return o.str();
    }

    Repository repo("");
    StatusResult st = repo.status();
    vector<string> files = repo.workingFiles();
    vector<CommitInfo> commits = repo.allHistory();
    vector<string> branches = repo.refs().branches();

    o << ",\"initialized\":true,\"user\":" << jstr(repo.getConfig("user.name"));
    o << ",\"branch\":" << jstr(repo.refs().currentBranch());
    o << ",\"head\":" << jstr(repo.refs().headCommit());
    o << ",\"merging\":" << (st.merging ? "true" : "false");
    o << ",\"canUndo\":" << (UndoLog::load(Repository::DIR + "/undo").empty() ? "false" : "true");
    o << ",\"canRedo\":" << (UndoLog::load(Repository::DIR + "/redo").empty() ? "false" : "true");

    o << ",\"staged\":[";
    for (size_t i = 0; i < st.staged.size(); ++i)
        o << (i ? "," : "") << "{\"s\":\"" << st.staged[i].first << "\",\"p\":" << jstr(st.staged[i].second) << "}";
    o << "],\"unstaged\":[";
    for (size_t i = 0; i < st.unstaged.size(); ++i)
        o << (i ? "," : "") << "{\"s\":\"" << st.unstaged[i].first << "\",\"p\":" << jstr(st.unstaged[i].second) << "}";
    o << "],\"untracked\":[";
    for (size_t i = 0; i < st.untracked.size(); ++i) o << (i ? "," : "") << jstr(st.untracked[i]);
    o << "],\"files\":[";
    for (size_t i = 0; i < files.size(); ++i) o << (i ? "," : "") << jstr(files[i]);
    o << "],\"branches\":[";
    for (size_t i = 0; i < branches.size(); ++i)
        o << (i ? "," : "") << "{\"name\":" << jstr(branches[i]) << ",\"tip\":" << jstr(repo.refs().branchTip(branches[i])) << "}";
    o << "],\"commits\":[";
    for (size_t i = 0; i < commits.size(); ++i) {
        const Commit& c = commits[i].commit;
        o << (i ? "," : "") << "{\"hash\":" << jstr(commits[i].hash) << ",\"parents\":[";
        for (size_t k = 0; k < c.parents.size(); ++k) o << (k ? "," : "") << jstr(c.parents[k]);
        o << "],\"author\":" << jstr(c.authorName()) << ",\"email\":" << jstr(c.authorEmail())
          << ",\"timestamp\":" << c.timestamp() << ",\"message\":" << jstr(util::rtrim(c.message)) << "}";
    }
    o << "]}";
    return o.str();
}

// Runs a whitelisted command through the very same code path as the CLI.
// Body format: first line = command name, following lines = its arguments.
string runApi(const string& body) {
    static const char* allowed[] = {"init", "config", "add", "commit", "checkout", "branch", "merge",
                                    "undo", "redo", "diff", "search", "stats", "verify", "log", "status"};
    vector<string> lines = util::split(body, '\n');
    for (size_t i = 0; i < lines.size(); ++i) lines[i] = util::split(lines[i], '\r')[0];
    while (!lines.empty() && lines.back().empty()) lines.pop_back();
    if (lines.empty()) throw MiniGitError("no command given");

    string name = lines[0];
    bool ok = false;
    for (size_t i = 0; i < sizeof(allowed) / sizeof(allowed[0]); ++i) if (name == allowed[i]) ok = true;
    if (!ok) throw MiniGitError("command '" + name + "' is not available in the interface");
    vector<string> args(lines.begin() + 1, lines.end());

    std::ostringstream out, err;
    std::streambuf* old = std::cout.rdbuf(out.rdbuf());     // capture what the command prints
    int rc;
    try { rc = runCommand(name, args, false, err); }
    catch (...) { std::cout.rdbuf(old); throw; }
    std::cout.rdbuf(old);

    return string("{\"ok\":") + (rc == 0 ? "true" : "false") + ",\"output\":" + jstr(out.str()) +
           ",\"error\":" + jstr(err.str()) + "}";
}

void handle(sock_t c) {
    Request r;
    if (!readRequest(c, r)) return;

    // Only answer requests addressed to this computer (blocks DNS-rebinding tricks).
    string host = r.headers["host"];
    if (!util::startsWith(host, "localhost") && !util::startsWith(host, "127.0.0.1")) {
        respondError(c, 403, "forbidden");
        return;
    }
    try {
        if (r.method == "GET" && (r.path == "/" || r.path == "/index.html" ||
                                  r.path == "/style.css" || r.path == "/app.js" ||
                                  r.path == "/editor" || r.path == "/editor.html" || r.path == "/editor.js")) {
            serveStatic(c, r.path == "/index.html" ? "/" : r.path);
            return;
        }
        if (!util::startsWith(r.path, "/api/")) { respondError(c, 404, "not found"); return; }
        // A custom header cannot be sent by other websites without our permission.
        if (r.headers["x-minigit"] != "1") { respondError(c, 403, "forbidden"); return; }

        if (r.method == "GET" && r.path == "/api/repos") {
            respond(c, 200, "application/json", repositoriesJson());
        } else if (r.method == "POST" && r.path == "/api/repos") {
            vector<string> fields = util::split(r.body, '\n');
            string name = fields.empty() ? "" : util::trim(fields[0]);
            string user = fields.size() > 1 ? util::trim(fields[1]) : "";
            string mail = fields.size() > 2 ? util::trim(fields[2]) : "";
            if (!validRepoName(name)) throw MiniGitError("repository name may contain only letters, numbers, spaces, '_' and '-'");
            string target = gRootDir + "/Repositories/" + name;
            if (fs::exists(target)) throw MiniGitError("a repository with that name already exists");
            fs::makeDirs(target);
            if (!fs::changeDir(target)) throw MiniGitError("could not open the new repository folder");
            gSelectedRepo = name;
            std::ostringstream err;
            if (runCommand("init", vector<string>(), false, err) != 0)
                throw MiniGitError("could not initialize repository: " + err.str());
            if (!user.empty()) runCommand("config", vector<string>{"user.name", user}, false, err);
            if (!mail.empty()) runCommand("config", vector<string>{"user.email", mail}, false, err);
            respond(c, 200, "application/json", "{\"ok\":true,\"name\":" + jstr(name) + "}");
        } else if (r.method == "POST" && r.path == "/api/select") {
            string name = util::trim(r.body);
            if (name.empty()) {
                if (!fs::changeDir(gRootDir)) throw MiniGitError("could not return to the project folder");
                gSelectedRepo.clear();
            } else {
                if (!validRepoName(name) || !fs::isDir(gRootDir + "/Repositories/" + name))
                    throw MiniGitError("repository not found");
                if (!fs::changeDir(gRootDir + "/Repositories/" + name)) throw MiniGitError("could not open repository");
                gSelectedRepo = name;
            }
            respond(c, 200, "application/json", "{\"ok\":true}");
        } else if (r.method == "GET" && r.path == "/api/state") {
            respond(c, 200, "application/json", stateJson());
        } else if (r.method == "POST" && r.path == "/api/run") {
            respond(c, 200, "application/json", runApi(r.body));
        } else if (r.method == "GET" && r.path == "/api/file") {
            string p = safePath(queryParam(r.query, "name"));
            respond(c, 200, "text/plain", fs::isFile(p) ? fs::readFile(p) : "");
        } else if (r.method == "POST" && r.path == "/api/save") {
            string p = safePath(queryParam(r.query, "name"));
            fs::writeFile(p, r.body);
            respond(c, 200, "application/json", "{\"ok\":true,\"name\":" + jstr(p) + "}");
        } else if (r.method == "POST" && r.path == "/api/delete") {
            string p = safePath(queryParam(r.query, "name"));
            if (!fs::isFile(p)) throw MiniGitError("file '" + p + "' does not exist");
            fs::removeFile(p);
            fs::removeEmptyParents(p);
            respond(c, 200, "application/json", "{\"ok\":true}");
        } else {
            respondError(c, 404, "unknown request");
        }
    } catch (const MiniGitError& e) {
        respondError(c, 400, e.what());
    } catch (const std::exception& e) {
        respondError(c, 500, e.what());
    }
}

void openInBrowser(const string& url) {
#ifdef _WIN32
    std::system(("start \"\" \"" + url + "\"").c_str());
#elif defined(__APPLE__)
    std::system(("open \"" + url + "\" >/dev/null 2>&1").c_str());
#else
    (void)url;       // on Linux just print the address
#endif
}

} // namespace

int runServer(int port, bool openBrowser) {
    gRootDir = fs::cwd();
    gSelectedRepo.clear();
    fs::makeDirs(gRootDir + "/Repositories");
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) throw MiniGitError("cannot start Windows networking");
#endif
    sock_t server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == MG_BAD_SOCK) throw MiniGitError("cannot create a network socket");
#ifndef _WIN32
    int yes = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
#endif

    // Listen on this computer only (127.0.0.1); try a few ports if one is busy.
    int chosen = 0;
    for (int p = port; p < port + 10 && !chosen; ++p) {
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons((unsigned short)p);
        if (bind(server, (struct sockaddr*)&addr, sizeof(addr)) == 0) chosen = p;
    }
    if (!chosen) throw MiniGitError("no free port found near " + util::toStr(port) + " (use --port <number>)");
    if (listen(server, 16) != 0) throw MiniGitError("cannot listen on the network port");

    string url = "http://localhost:" + util::toStr(chosen);
    std::cout << "\n  minigit interface is running\n"
              << "  Folder : " << fs::cwd() << "\n"
              << "  Address: " << url << "\n\n"
              << "  Keep this window open. Press Ctrl+C to stop.\n" << std::endl;
    if (openBrowser) openInBrowser(url);

    while (true) {
        struct sockaddr_in peer;
        mg_socklen len = sizeof(peer);
        sock_t c = accept(server, (struct sockaddr*)&peer, &len);
        if (c == MG_BAD_SOCK) continue;
        // Browsers sometimes open idle "spare" connections; do not wait on them for long.
#ifdef _WIN32
        DWORD ms = 1500;
        setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, (const char*)&ms, sizeof(ms));
#else
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 500000;
        setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#endif
        try { handle(c); } catch (...) {}
        MG_CLOSE(c);
    }
    return 0;
}
