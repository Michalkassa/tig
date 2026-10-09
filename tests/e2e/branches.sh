#!/bin/sh
# Branches, checkout, HEAD and the graph.
. "$(dirname "$0")/lib.sh"

"$TIG" init >/dev/null
check_fails "branch before first commit" "$TIG" branch early

echo 1 > a.txt
"$TIG" add a.txt
"$TIG" commit -m first >/dev/null
echo 2 >> a.txt
"$TIG" add a.txt
"$TIG" commit -m second >/dev/null
base="$(cat .tig/refs/main)"

# Creating a branch points it at the current commit; HEAD stays on main.
"$TIG" branch feature
check "branch created at HEAD" sh -c "[ \"\$(cat .tig/refs/feature)\" = $base ]"
check "still on main" grep -q "^ref: main$" .tig/HEAD
check "branch list marks current" sh -c "'$TIG' branch | grep -q '^\* main$'"
check_fails "duplicate branch" "$TIG" branch feature
check_fails "invalid branch name" "$TIG" branch "bad/name"

# Checkout moves HEAD; commits advance only the current branch.
"$TIG" checkout feature >/dev/null
check "HEAD on feature" grep -q "^ref: feature$" .tig/HEAD
check "status shows branch" status_has "^On branch feature$"
echo f > f.txt
"$TIG" add f.txt
"$TIG" commit -m "feature work" >/dev/null
feat="$(cat .tig/refs/feature)"
check "feature advanced" sh -c "[ $feat != $base ]"
check "main unchanged" sh -c "[ \"\$(cat .tig/refs/main)\" = $base ]"
check "feature commit points to base" grep -q "^parent $base\$" ".tig/commits/$feat"

# Switching back restores main's snapshot in the working tree.
"$TIG" checkout main >/dev/null
check "f.txt removed on main" test ! -e f.txt
check "main tree clean" status_has "working tree clean"
"$TIG" checkout feature >/dev/null
check "f.txt restored on feature" grep -qx f f.txt
"$TIG" checkout main >/dev/null

# Nested files get their directories recreated and pruned.
"$TIG" checkout -b nested >/dev/null
check "checkout -b switches" grep -q "^ref: nested$" .tig/HEAD
mkdir -p d/e && echo n > d/e/n.txt
"$TIG" add d
"$TIG" commit -m nested >/dev/null
"$TIG" checkout main >/dev/null
check "nested dirs pruned" test ! -e d
"$TIG" checkout nested >/dev/null
check "nested file restored" grep -qx n d/e/n.txt
"$TIG" checkout main >/dev/null

# Uncommitted work blocks checkout instead of being lost.
echo dirty >> a.txt
check_fails "checkout with unstaged change" "$TIG" checkout feature
"$TIG" add a.txt
check_fails "checkout with staged change" "$TIG" checkout feature
"$TIG" commit -m third >/dev/null
echo mine > f.txt
check_fails "untracked file would be overwritten" "$TIG" checkout feature
check "untracked file untouched" grep -qx mine f.txt
rm f.txt

# Deleting branches.
check_fails "delete current branch" "$TIG" branch -d main
"$TIG" branch -d nested >/dev/null
check "branch deleted" test ! -e .tig/refs/nested
check_fails "delete missing branch" "$TIG" branch -d nested

# Detached HEAD by commit hash.
"$TIG" checkout "$(echo "$base" | cut -c1-7)" >/dev/null
check "HEAD detached" grep -q "^$base$" .tig/HEAD
check "status shows detached" status_has "^HEAD detached at"
check "a.txt at base content" sh -c "[ \"\$(cat a.txt)\" = \"\$(printf '1\n2')\" ]"
"$TIG" checkout main >/dev/null
check "back on main" grep -q "^ref: main$" .tig/HEAD

# Graph: main is a straight line on the left, feature forks off base.
short() { echo "$1" | cut -c1-7; }
graph="$("$TIG" graph)"
expected="* $(short "$(cat .tig/refs/main)") (HEAD -> main) third
| * $(short "$feat") (feature) feature work
|/
* $(short "$base") second
* $(short "$(grep '^parent ' ".tig/commits/$base" | cut -c8-)") first"
check "graph layout" [ "$graph" = "$expected" ]
[ "$graph" = "$expected" ] || printf 'got:\n%s\nexpected:\n%s\n' "$graph" "$expected"

# Three branches from one commit fold back one lane at a time.
"$TIG" branch b2
"$TIG" checkout b2 >/dev/null
echo b2 > b2.txt && "$TIG" add b2.txt && "$TIG" commit -m b2 >/dev/null
"$TIG" checkout main >/dev/null
"$TIG" checkout "$(short "$base")" >/dev/null
"$TIG" checkout -b b3 >/dev/null
echo b3 > b3.txt && "$TIG" add b3.txt && "$TIG" commit -m b3 >/dev/null
graph="$("$TIG" graph)"
check "fan-out folds into two collapse rows" sh -c "echo \"\$1\" | grep -qx '| |/' && echo \"\$1\" | grep -qx '|/'" _ "$graph"
check "graph shows detached HEAD" sh -c "'$TIG' checkout '$(short "$base")' >/dev/null && '$TIG' graph | grep -q '(HEAD) second'"

finish
