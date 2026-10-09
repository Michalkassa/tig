#!/bin/sh
# File life cycle: untracked -> staged -> unmodified -> modified -> ...
. "$(dirname "$0")/lib.sh"

"$TIG" init >/dev/null
check "init creates .tig" test -d .tig/objects
check "init starts on main" grep -q "^ref: main$" .tig/HEAD
check_fails "init twice" "$TIG" init

# untracked
echo hello > a.txt
echo world > b.txt
check "a.txt untracked" in_section Untracked "a.txt"

# untracked -> add -> staged
"$TIG" add a.txt b.txt
check "a.txt staged as new" in_section Staged "new file: +a.txt"
check "blob stored under its sha1" test -f ".tig/objects/$(sha1_of a.txt)"
check_fails "add missing file" "$TIG" add nope.txt

# staged -> commit -> unmodified
"$TIG" commit -m "first" >/dev/null
check "a.txt unmodified" in_section Unmodified "a.txt"
check "clean tree" status_has "working tree clean"
check_fails "empty commit refused" "$TIG" commit -m again
check_fails "commit without message" "$TIG" commit
c1="$(cat .tig/refs/main)"

# unmodified -> edit -> modified
echo changed >> a.txt
check "a.txt modified" in_section Modified "modified: +a.txt"

# modified -> add -> staged
"$TIG" add a.txt
check "a.txt staged as modified" in_section Staged "modified: +a.txt"
check "b.txt still unmodified" in_section Unmodified "b.txt"
"$TIG" commit -m "second" >/dev/null
c2="$(cat .tig/refs/main)"

# Unchanged file in new snapshot points at the same blob as before.
check "b.txt reuses blob" sh -c "[ \"\$(grep ' b.txt\$' .tig/commits/$c1)\" = \"\$(grep ' b.txt\$' .tig/commits/$c2)\" ]"
check "a.txt has new blob" sh -c "[ \"\$(grep ' a.txt\$' .tig/commits/$c1)\" != \"\$(grep ' a.txt\$' .tig/commits/$c2)\" ]"
check "commit points to previous" grep -q "^parent $c1\$" ".tig/commits/$c2"

# unmodified -> rm -> untracked
"$TIG" rm b.txt >/dev/null
check "b.txt staged for removal" in_section Staged "removed: +b.txt"
check "b.txt file kept on disk" test -f b.txt
"$TIG" commit -m "untrack b" >/dev/null
check "b.txt untracked after commit" in_section Untracked "b.txt"
check_fails "rm untracked file" "$TIG" rm b.txt

# Subdirectories, and running from inside one.
mkdir -p sub/deep
echo x > sub/deep/c.txt
(cd sub && "$TIG" add deep)
check "add from subdirectory" in_section Staged "new file: +sub/deep/c.txt"
check_fails "path escaping repo" "$TIG" add ../x

# Deleting a tracked file and staging that deletion.
"$TIG" commit -m "add c" >/dev/null
rm sub/deep/c.txt
check "deleted file shown" in_section Modified "deleted: +sub/deep/c.txt"
"$TIG" add .
check "deletion staged" in_section Staged "removed: +sub/deep/c.txt"

check "log lists 4 commits" sh -c "[ \$('$TIG' log | grep -c '^commit ') -eq 4 ]"
check "show by short hash" sh -c "'$TIG' show \$(echo $c2 | cut -c1-7) | grep -q 'unchanged  b.txt'"

finish
