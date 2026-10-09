# Shared helpers for end-to-end tests. Source from a test script.
set -eu

TIG="${TIG:-$(cd "$(dirname "$0")/../.." && pwd)/tig}"
export NO_COLOR=1
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
cd "$WORK"

pass=0
fail=0

# check <description> <command...>: passes if the command exits 0.
check() {
    desc="$1"; shift
    if "$@" >/dev/null 2>&1; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
        echo "FAIL: $desc"
    fi
}

# Same, but the command is expected to fail.
check_fails() {
    desc="$1"; shift
    if "$@" >/dev/null 2>&1; then
        fail=$((fail + 1))
        echo "FAIL (expected an error): $desc"
    else
        pass=$((pass + 1))
    fi
}

sha1_of() {
    if command -v sha1sum >/dev/null 2>&1; then sha1sum "$1"; else shasum "$1"; fi | cut -c1-40
}

status_has() { "$TIG" status | grep -qE "$1"; }

# Lines of one status section (Staged / Modified / Untracked / Unmodified).
section() {
    "$TIG" status | awk -v s="$1" '/^[A-Z]/{on=index($0,s)==1; next} on && NF'
}
in_section() { section "$1" | grep -qE "$2"; }

finish() {
    echo "$(basename "$0"): $pass passed, $fail failed"
    [ "$fail" -eq 0 ]
}
