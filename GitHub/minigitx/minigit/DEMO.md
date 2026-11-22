# Demo script (about 5 minutes)

Build once: `build.bat`   |   Start: `minigit ui D:\demo-project`   (browser opens)

(Qt version: open `qt/minigit-qt.pro` in Qt Creator, Ctrl+R, choose the folder. The same steps below work there.
If the Qt version gives any trouble, use the web version above.)

1. **Initialize** - type a name and email, click *Initialize repository*.
2. **Write data into a file** - click *Open file editor*. A second page opens in a new tab. Type a file name such as
   `notes.txt`, write some text, press *Save file* (or Ctrl+S). Go back to the first tab: it now says "1 new" is waiting.
3. **Stage + commit** - click *Stage all changes*, write a message, *Commit*. The history graph gets its first dot.
   Point at the panel at the bottom: it shows the real command (`minigit commit -m ...`).
4. **Change the file** in the editor tab and save, then click *Show changes* - coloured diff (this is the LCS algorithm).
5. **Branch** - type `feature`, *Create & switch*. In the editor create `feature.txt`, then stage all and commit.
   The graph now shows two lines.
6. **Merge** - switch back to `master`, make a commit there, then pick `feature` and *Merge into current*.
   The graph shows a merge commit with two parents.
7. **Undo / Redo** - click *Undo*: the merge disappears; *Redo* brings it back.
8. **Search** - type a word prefix such as `feat` (trie lookup).
9. **Verify** - checks every object's SHA-1. Then show real Git reading it:
   `git --git-dir=D:\demo-project\\.minigit log --oneline --graph --all`
   and `git --git-dir=D:\demo-project\\.minigit fsck`.

## Questions you may be asked
- *Why SHA-1 and a hash map?* Content-addressable storage: same content = same hash = stored once.
- *Where is a graph used?* Commits form a DAG (a merge has two parents); merge-base uses BFS.
- *How does diff work?* Longest Common Subsequence with dynamic programming.
- *Where is a heap used?* Newest-first log over a DAG, and top-k in `stats`.
- *Where is a trie used?* The `search` command (prefix lookup in O(length of word)).
- *How does undo work?* Every operation saves the old branch pointers on a stack; undo pops it.
- *OOP?* `Object` base class with `Blob`, `Tree`, `Commit`; one `Command` class per command.

## If you are asked "how is the interface built?"
- **Web version:** `ui/index.html` (structure), `ui/style.css` (looks), `ui/app.js` (behaviour). The page talks to
  minigit.exe through a tiny built-in web server on 127.0.0.1 (`src/Server.cpp`); each button runs the same
  command classes as the command line.
- **Qt version:** `qt/MainWindow.cpp` builds the window with Qt widgets and calls the same `Repository` and
  `runCommand` code directly (no browser, no subprocess).
