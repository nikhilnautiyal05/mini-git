#!/bin/bash
# End-to-end test. Uses real Git (if installed) to confirm that minigit writes
# objects in the genuine Git format.   Usage: tests/test.sh [path-to-minigit]
MG="$(cd "$(dirname "${1:-./minigit}")" && pwd)/$(basename "${1:-./minigit}")"
T=$(mktemp -d); cd "$T"
pass=0; fail=0
ok()   { echo "  PASS  $1"; pass=$((pass+1)); }
bad()  { echo "  FAIL  $1"; fail=$((fail+1)); }
check(){ if [ "$2" = "$3" ]; then ok "$1"; else bad "$1 (expected '$2', got '$3')"; fi; }

echo "== hashing"
printf 'hello\n' > hello.txt
check "blob hash equals Git's" "ce013625030ba8dba906f756967f9e9ca394464a" "$($MG init >/dev/null 2>&1; $MG hash-object hello.txt)"
if command -v git >/dev/null; then
  check "matches 'git hash-object'" "$(git hash-object hello.txt)" "$($MG hash-object hello.txt)"
fi

echo "== commit / log / status"
$MG config user.name "Test User" ; $MG config user.email "test@example.com"
mkdir -p src/deep; echo 'int main(){}' > src/main.cpp; echo 'x' > src/deep/a.txt
$MG add . ; $MG commit -m "first commit" >/dev/null
check "status clean" "nothing to commit, working tree clean" "$($MG status | tail -1)"
echo 'more' >> hello.txt; echo new > new.txt
$MG status | grep -q "modified:   hello.txt" && ok "detects modified file" || bad "detects modified file"
$MG status | grep -q "new.txt" && ok "detects untracked file" || bad "detects untracked file"
$MG add hello.txt new.txt; $MG commit -m "second commit" >/dev/null
check "log has 2 commits" "2" "$($MG log --oneline | wc -l | tr -d ' ')"
check "HEAD~1 is first" "first commit" "$($MG log --oneline HEAD~1 -n 1 | cut -d' ' -f2-)"

echo "== real Git can read our repository"
if command -v git >/dev/null; then
  G="git --git-dir=.minigit"
  check "git log subjects" "second commit
first commit" "$($G log --format=%s)"
  check "git ls-tree (nested folders)" "src/deep/a.txt" "$($G ls-tree -r --name-only HEAD | grep deep)"
  check "git cat-file blob" "int main(){}" "$($G show HEAD:src/main.cpp)"
  check "git author" "Test User <test@example.com>" "$($G log -1 --format='%an <%ae>')"
  if $G fsck --no-dangling --no-progress 2>&1 | grep -qiE "error|corrupt|missing"; then bad "git fsck"; else ok "git fsck finds no corruption"; fi
  check "tree hash equals git's" "$($G rev-parse HEAD^{tree})" "$($MG cat-file -p HEAD | head -1 | cut -d' ' -f2)"
fi
$MG verify | grep -q "intact" && ok "verify" || bad "verify"

echo "== diff"
echo 'changed' > hello.txt
$MG diff | grep -q '^+changed' && ok "diff shows added line" || bad "diff shows added line"
$MG add hello.txt; $MG commit -m "third" >/dev/null
if command -v git >/dev/null; then
  check "log order matches git even for same-second commits" "$(git --git-dir=.minigit log --format=%s)" "$($MG log --oneline | cut -d' ' -f2-)"
fi
$MG diff HEAD~1 HEAD | grep -q '^@@' && ok "diff between commits" || bad "diff between commits"

echo "== branches / checkout / merge"
$MG checkout -b feature >/dev/null
echo feat > feature.txt; $MG add feature.txt; $MG commit -m "add feature" >/dev/null
$MG checkout master >/dev/null
[ ! -f feature.txt ] && ok "checkout removes branch-only file" || bad "checkout removes branch-only file"
$MG merge feature | grep -q "Fast-forward" && ok "fast-forward merge" || bad "fast-forward merge"
[ -f feature.txt ] && ok "merged file present" || bad "merged file present"

$MG checkout -b left >/dev/null; echo L > left.txt; $MG add left.txt; $MG commit -m "left" >/dev/null
$MG checkout master >/dev/null; echo R > right.txt; $MG add right.txt; $MG commit -m "right" >/dev/null
$MG merge left >/dev/null
check "three-way merge makes 2-parent commit" "2" "$(git --git-dir=.minigit log -1 --format=%p | wc -w | tr -d ' ')"
[ -f left.txt ] && [ -f right.txt ] && ok "both sides present after merge" || bad "both sides present after merge"

$MG checkout -b c1 >/dev/null; echo ONE > clash.txt; $MG add clash.txt; $MG commit -m c1 >/dev/null
$MG checkout master >/dev/null; $MG checkout -b c2 >/dev/null; echo TWO > clash.txt; $MG add clash.txt; $MG commit -m c2 >/dev/null
$MG merge c1 >/dev/null 2>&1; rc=$?
check "conflict returns non-zero" "1" "$rc"
grep -q '<<<<<<<' clash.txt && ok "conflict markers written" || bad "conflict markers written"
$MG merge --abort >/dev/null && ok "merge --abort" || bad "merge --abort"

echo "== undo / redo"
$MG checkout master >/dev/null
before=$($MG log --oneline | wc -l)
echo u > undo.txt; $MG add undo.txt; $MG commit -m "to undo" >/dev/null
$MG undo >/dev/null
check "undo removes the commit" "$before" "$($MG log --oneline | wc -l)"
$MG redo >/dev/null
check "redo brings it back" "$((before+1))" "$($MG log --oneline | wc -l)"

echo "== search / stats / json"
$MG search undo | grep -q "to undo" && ok "search finds commit by prefix" || bad "search finds commit by prefix"
$MG stats | grep -q "Commits:" && ok "stats" || bad "stats"
$MG log --json | python3 -c 'import json,sys; json.load(sys.stdin)' 2>/dev/null && ok "log --json is valid JSON" || bad "log --json is valid JSON"
$MG status --json | python3 -c 'import json,sys; json.load(sys.stdin)' 2>/dev/null && ok "status --json is valid JSON" || bad "status --json is valid JSON"

echo "== corruption is detected"
obj=$(find .minigit/objects -type f | head -1); printf 'junk' >> "$obj"
$MG verify >/dev/null 2>&1 && bad "verify detects tampering" || ok "verify detects tampering"

echo; echo "passed: $pass   failed: $fail"
cd /; rm -rf "$T"; [ $fail -eq 0 ]
