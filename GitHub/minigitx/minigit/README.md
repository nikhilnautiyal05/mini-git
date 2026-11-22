# minigit

A small version control system written in C++11 that follows the **real Git
format**: it writes the same objects, hashes, trees, commits, refs and index
that Git does, so real Git can read a minigit repository (and the other way
round).

Built as a Data Structures + OOP project.

## Build

**Windows (MinGW g++, any version that supports C++11):**
```
build.bat
```
**Linux / macOS / MSYS2:**
```
make
```
Both produce `minigit` / `minigit.exe`. No external libraries are needed
(SHA-1 and zlib are implemented in the project).

## Quick start
```
minigit init
minigit config user.name "Your Name"
minigit config user.email "you@example.com"

minigit add .
minigit commit -m "first commit"
minigit status
minigit log --oneline
```
The repository lives in a `.minigit` folder inside the project folder you ran
`init` in. Every project folder has its own.

## Commands
| Command | What it does |
|---|---|
| `init` | create `.minigit` in this folder |
| `config <key> [value]` | get/set `user.name`, `user.email` |
| `add <path>...` | stage files or folders (`.` = everything) |
| `commit -m "msg"` | save a snapshot |
| `status [--json]` | staged / modified / untracked files |
| `log [rev] [--oneline] [-n N] [--json]` | history, newest first |
| `diff`, `diff --staged`, `diff <a> <b>` | unified line diffs |
| `branch [name]`, `branch -d/-D name` | list / create / delete branches |
| `checkout [-b] <branch or rev>` | switch branch or commit, create a branch |
| `merge <branch>`, `merge --abort` | fast-forward and three-way merge |
| `undo`, `redo` | reverse / replay the last commit, branch, checkout or merge |
| `search <word>...` | find commits by message/author prefix (trie) |
| `stats` | commits, authors, most-changed files |
| `verify` | recompute every object's hash and check all links |
| `cat-file -t/-s/-p <rev>` | inspect objects (same flags as Git) |
| `hash-object [-w] <file>` | blob hash of a file |
| `ui [folder] [--port N]` | open the graphical web interface in your browser |

Revisions: `HEAD`, `HEAD~2`, a branch name, or a full/short hash.
A `.minigitignore` file (names, `dir/`, `*.ext`) hides files from `add`/`status`.

## Graphical interfaces (two options)

### A. Web interface (HTML + CSS + a little JavaScript)
```
minigit ui                 (uses the current folder)
minigit ui D:\my-project   (any folder; it is created if missing)
```
Your browser opens `http://localhost:8080`. Keep the terminal window open
(Ctrl+C stops it).

The page is three plain files in the `ui/` folder, which minigit.exe loads
from the folder next to it:

| File | Contains |
|---|---|
| `ui/index.html` | the main page: stage, commit, branches, history (commented) |
| `ui/editor.html` | a second page, opened in a new tab, for writing data into files |
| `ui/style.css` | all colours, sizes and layout (shared by both pages) |
| `ui/app.js` | behaviour of the main page (buttons, drawing the commit graph) |
| `ui/editor.js` | behaviour of the editor page (list, open, save, delete) |

Edit a file, save it, press F5 in the browser - no rebuild needed. If the `ui`
folder is missing, copies built into the program are used.
(`python tools/embed_ui.py` rebuilds those embedded copies if you want your edits built in.)

It only listens on your own computer (127.0.0.1).

### B. Qt desktop application (C++)
Source in `qt/`. It calls the same C++ classes as the command line (no browser,
no subprocess). It needs Qt installed (Qt 5.9+ or Qt 6, with the MinGW kit).

1. Open `qt/minigit-qt.pro` in **Qt Creator**.
2. Choose a Desktop kit (MinGW) and click **Configure Project**.
3. Press **Ctrl+R** (Build and Run). A folder chooser opens - pick your project folder.

Command line alternative (from a Qt command prompt): `cd qt`, `qmake`, then `mingw32-make`.

The Qt window has the same flow as the web page (its editor is built into the same window): files with status, editor,
stage, commit, branches and merge, history table (with parents and branch
names, so merges are visible), coloured diffs, undo/redo, verify, stats, and
an output panel. It does not draw the graph picture; the web interface does.

## Where the DSA and OOP concepts are
| Concept | Where |
|---|---|
| Linked structure / DAG | commits point to parents; merges have two parents (`Repository::history`, `mergeBase`) |
| Hash function / hash-addressing | SHA-1 object ids, objects stored once by hash (`Sha1`, `ObjectStore`) |
| Tree | directory snapshots as nested `Tree` objects (`writeTree`, `flattenTree`) |
| Red-black tree (`std::map`) | index and snapshots (`FileMap`) |
| Priority queue (max-heap) | newest-first log over a DAG; top-k in `stats` |
| BFS / DFS | merge-base search, ancestor checks |
| Dynamic programming (LCS) | `diff` (`Diff.cpp`) |
| Trie | `search` (`Trie.cpp`) |
| Stack | undo / redo stacks (`Undo.cpp`) |
| Merkle structure | hashes of hashes, checked by `verify` |
| Inheritance + polymorphism | `Object` -> `Blob`, `Tree`, `Commit` (`Objects.h`) |
| Command pattern | one class per command (`Commands.cpp`) |
| Encapsulation | `Repository`, `ObjectStore`, `Refs`, `Index` each own one job |

## Layout
```
include/  headers     src/  implementation     ui/  web page (html, css, js)
qt/       Qt desktop app     tools/  helper script     tests/test.sh  end-to-end test
```
Main pieces: `Server` + `UiPage` + `ui/` (web interface), `qt/` (Qt desktop interface), `Sha1`, `Zlib` (stored deflate + full inflate), `Objects`,
`ObjectStore`, `Index` (binary DIRC v2), `Refs`, `Repository`, `Diff`, `Trie`,
`Undo`, `Commands`, `main`.

## Compatibility with real Git
- Object ids, tree/commit byte formats, `HEAD`, `refs/heads/*`, the binary
  index and the INI `config` all match Git.
- Try it: `git --git-dir=.minigit log`, `git --git-dir=.minigit fsck`,
  `git --git-dir=.minigit cat-file -p HEAD`.
- minigit can also read repositories made by real Git (loose objects).

## Limitations (honest list)
- Objects are written uncompressed-in-zlib ("stored" blocks), so they are larger than Git's.
- No packfiles, remotes, tags or stash. Packed repositories can't be read.
- Merge works per file: if both sides changed the same file, the whole file gets
  conflict markers (no line-level merge). Undo of checkout/merge needs a clean working folder.
- All files are saved as mode `100644`; symlinks and executable bits are not tracked.
- Run `commit` after resolving a conflict with `add`; `merge --abort` discards the merge.

## Tests
```
make && bash tests/test.sh ./minigit
```
(Needs bash; uses real `git` too if installed. On Windows run it in Git Bash.)

## Putting it on GitHub
```
git init
git add .
git commit -m "minigit"
git remote add origin https://github.com/<you>/<repo>.git
git push -u origin main
```
`.gitignore` already skips the binary and `.minigit/`.

## Multiple repositories in the GUI

When you start the graphical interface with `minigit ui`, MiniGitX creates a
`Repositories` folder beside the program's launch folder. From the repository
manager screen, enter a repository name and choose **Create repository**, or
select an existing repository and choose **Open repository**.

Each repository is stored independently:

```text
Repositories/
  CollegeProject/
    .minigit/
    your source files...
  Practice/
    .minigit/
    your source files...
```

Use the **Switch** or **Repositories** controls in the main interface to move
between repositories. The file editor writes files into the currently selected
repository. Existing files and any `.minigit` folder in the launch directory are
left untouched; newly created GUI repositories are placed under `Repositories`.

After changing the source, rebuild on Windows by running `build.bat` with
MinGW g++ installed. The generated `minigit.exe` must be the one you launch.
