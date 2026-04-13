#!/usr/bin/env bash
# cpin test runner — tests the cpin binary end-to-end via CLI
# Usage: ./tests/run_tests.sh [path/to/cpin]

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(dirname "$SCRIPT_DIR")"
# Resolve to absolute path before any cd changes the working directory.
_raw_bin="${1:-$REPO_DIR/bin/cpin}"
BINARY="$(cd "$(dirname "$_raw_bin")" 2>/dev/null && pwd)/$(basename "$_raw_bin")"

# ── colors ────────────────────────────────────────────────────────────────────
if [ -t 1 ]; then
    RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
    CYAN='\033[0;36m'; BOLD='\033[1m'; RESET='\033[0m'
else
    RED=''; GREEN=''; YELLOW=''; CYAN=''; BOLD=''; RESET=''
fi

# ── state ─────────────────────────────────────────────────────────────────────
PASS=0; FAIL=0

# ── helpers ───────────────────────────────────────────────────────────────────

# Run cpin from the temp dir; store output in $OUT, exit code in $RC
cpin() {
    OUT=$("$BINARY" "$@" 2>&1)
    RC=$?
}

assert_pass() {
    local desc="$1"; shift
    cpin "$@"
    if [ "$RC" -eq 0 ]; then
        echo -e "  ${GREEN}PASS${RESET}  $desc"
        ((PASS++))
    else
        echo -e "  ${RED}FAIL${RESET}  $desc"
        echo -e "        ${YELLOW}exit $RC${RESET}  output: $OUT"
        ((FAIL++))
    fi
}

assert_fail() {
    local desc="$1"; shift
    cpin "$@"
    if [ "$RC" -ne 0 ]; then
        echo -e "  ${GREEN}PASS${RESET}  $desc"
        ((PASS++))
    else
        echo -e "  ${RED}FAIL${RESET}  $desc"
        echo -e "        ${YELLOW}exit $RC${RESET}  output: $OUT"
        ((FAIL++))
    fi
}

assert_output_contains() {
    local desc="$1" pattern="$2"; shift 2
    cpin "$@"
    if echo "$OUT" | grep -qF "$pattern"; then
        echo -e "  ${GREEN}PASS${RESET}  $desc"
        ((PASS++))
    else
        echo -e "  ${RED}FAIL${RESET}  $desc"
        echo -e "        ${YELLOW}expected to contain:${RESET} $pattern"
        echo -e "        ${YELLOW}actual output:${RESET}       $OUT"
        ((FAIL++))
    fi
}

assert_output_not_contains() {
    local desc="$1" pattern="$2"; shift 2
    cpin "$@"
    if ! echo "$OUT" | grep -qF "$pattern"; then
        echo -e "  ${GREEN}PASS${RESET}  $desc"
        ((PASS++))
    else
        echo -e "  ${RED}FAIL${RESET}  $desc"
        echo -e "        ${YELLOW}should NOT contain:${RESET} $pattern"
        echo -e "        ${YELLOW}actual output:${RESET}      $OUT"
        ((FAIL++))
    fi
}

assert_exit_and_contains() {
    local desc="$1" expected_rc="$2" pattern="$3"; shift 3
    cpin "$@"
    local rc_ok=0 out_ok=0
    [ "$RC" -eq "$expected_rc" ] && rc_ok=1
    echo "$OUT" | grep -qF "$pattern" && out_ok=1
    if [ "$rc_ok" -eq 1 ] && [ "$out_ok" -eq 1 ]; then
        echo -e "  ${GREEN}PASS${RESET}  $desc"
        ((PASS++))
    else
        echo -e "  ${RED}FAIL${RESET}  $desc"
        [ "$rc_ok" -eq 0 ] && echo -e "        ${YELLOW}exit: got $RC, want $expected_rc${RESET}"
        [ "$out_ok" -eq 0 ] && echo -e "        ${YELLOW}expected to contain:${RESET} $pattern"
        echo -e "        ${YELLOW}actual output:${RESET} $OUT"
        ((FAIL++))
    fi
}

# ── pre-flight ────────────────────────────────────────────────────────────────

if [ ! -x "$BINARY" ]; then
    echo -e "${RED}error:${RESET} binary not found or not executable: $BINARY"
    echo "  run 'make' first, or pass the binary path as the first argument"
    exit 1
fi

# ── temp workspace ────────────────────────────────────────────────────────────
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

# Create a fake source file with 20 lines so line-number checks work.
# Line 8 is intentionally blank to trigger CPIN_WARN_EMPTY_LINE.
mkdir -p "$TMP_DIR/src"
cat > "$TMP_DIR/src/main.c" <<'EOF'
#include <stdio.h>
#include <stdlib.h>

int helper(int x) {
    return x * 2;
}

EOF
printf 'int main(void) {\n' >> "$TMP_DIR/src/main.c"
for i in $(seq 9 20); do
    printf '    /* line %d */\n' "$i" >> "$TMP_DIR/src/main.c"
done
printf '    return 0;\n}\n' >> "$TMP_DIR/src/main.c"

# All cpin calls run from inside TMP_DIR so .cpin/notes lands there.
cd "$TMP_DIR"

# ── tests ─────────────────────────────────────────────────────────────────────

echo -e "\n${BOLD}${CYAN}── argument / usage errors ──────────────────────────────────────────────${RESET}"

assert_exit_and_contains \
    "no args prints usage and exits 1" \
    1 "Usage:"

assert_exit_and_contains \
    "unknown command prints message" \
    0 "Unknown command:"  \
    unknowncmd

assert_exit_and_contains \
    "'add' with no target/note prints usage and exits 1" \
    1 "Usage:" \
    add

assert_exit_and_contains \
    "'remove' with no target prints usage and exits 1" \
    1 "Usage:" \
    remove

assert_exit_and_contains \
    "'search' with no keyword prints usage and exits 1" \
    1 "Usage:" \
    search

echo -e "\n${BOLD}${CYAN}── add: validation ──────────────────────────────────────────────────────${RESET}"

assert_exit_and_contains \
    "add to non-existent file fails" \
    1 "Error:" \
    add "no_such_file.c:1" "a note"

assert_exit_and_contains \
    "add with empty note content fails" \
    1 "Error:" \
    add "src/main.c:1" ""

assert_exit_and_contains \
    "add with line number out of bounds fails" \
    1 "Error:" \
    add "src/main.c:9999" "way too far"

assert_exit_and_contains \
    "add to blank line emits warning but still saves" \
    0 "Warning:" \
    add "src/main.c:7" "note on blank line"

echo -e "\n${BOLD}${CYAN}── add: happy path ──────────────────────────────────────────────────────${RESET}"

assert_exit_and_contains \
    "add note to src/main.c:1 succeeds" \
    0 "Note added:" \
    add "src/main.c:1" "why does this include stdlib?"

assert_exit_and_contains \
    "add second note to src/main.c:5 succeeds" \
    0 "Note added:" \
    add "src/main.c:5" "helper doubles the value"

assert_exit_and_contains \
    "add note with colon in content succeeds" \
    0 "Note added:" \
    add "src/main.c:9" "todo: fix this later"

echo -e "\n${BOLD}${CYAN}── add: duplicates ──────────────────────────────────────────────────────${RESET}"

assert_exit_and_contains \
    "exact duplicate note (same file+line+content) is blocked" \
    1 "Error:" \
    add "src/main.c:1" "why does this include stdlib?"

assert_exit_and_contains \
    "same file+line but different content emits warning and saves" \
    0 "Warning:" \
    add "src/main.c:1" "also: we could use stdint here"

echo -e "\n${BOLD}${CYAN}── list ─────────────────────────────────────────────────────────────────${RESET}"

assert_output_contains \
    "'list' with no args shows all notes" \
    "src/main.c:1:why does this include stdlib?" \
    list

assert_output_contains \
    "'list <file>' shows notes for that file" \
    "helper doubles the value" \
    list src/main.c

assert_output_contains \
    "'list <file> <line>' filters to that line" \
    "src/main.c:5:helper doubles the value" \
    list src/main.c 5

assert_exit_and_contains \
    "'list <file> <line>' with no matching notes prints informative message" \
    0 "No notes found for" \
    list src/main.c 99

assert_exit_and_contains \
    "'list' for a completely unknown file prints informative message" \
    0 "No notes found for" \
    list src/parser.c

echo -e "\n${BOLD}${CYAN}── search ───────────────────────────────────────────────────────────────${RESET}"

assert_output_contains \
    "'search' finds notes containing keyword" \
    "why does this include stdlib?" \
    search "stdlib"

assert_output_contains \
    "'search' keyword match is substring-based" \
    "todo: fix this later" \
    search "todo"

assert_exit_and_contains \
    "'search' with no matching keyword prints informative message" \
    0 "No notes matching" \
    search "xyzzy_not_found_anywhere"

echo -e "\n${BOLD}${CYAN}── export ───────────────────────────────────────────────────────────────${RESET}"

assert_output_contains \
    "'export' plain text shows all notes" \
    "src/main.c" \
    export

assert_output_contains \
    "'export --json' outputs a JSON array" \
    '"file"' \
    export --json

assert_output_contains \
    "'export --json' includes note text as JSON" \
    '"note"' \
    export --json

assert_output_contains \
    "'export --md' outputs a markdown header" \
    "## src/main.c" \
    export --md

assert_exit_and_contains \
    "'export --json --md' is rejected as mutually exclusive" \
    1 "mutually exclusive" \
    export --json --md

echo -e "\n${BOLD}${CYAN}── remove ───────────────────────────────────────────────────────────────${RESET}"

assert_exit_and_contains \
    "'remove' an existing note succeeds" \
    0 "Note removed:" \
    remove "src/main.c:5"

assert_exit_and_contains \
    "removed note no longer appears in list" \
    0 "No notes found for" \
    list src/main.c 5

assert_exit_and_contains \
    "'remove' a non-existent note fails" \
    1 "Error:" \
    remove "src/main.c:5"

assert_exit_and_contains \
    "'remove' for an unknown file fails" \
    1 "Error:" \
    remove "src/no_such.c:1"

# ── summary ───────────────────────────────────────────────────────────────────

TOTAL=$((PASS + FAIL))
echo -e "\n${BOLD}── results ───────────────────────────────────────────────────────────────${RESET}"
printf "  %d / %d tests passed" "$PASS" "$TOTAL"
if [ "$FAIL" -eq 0 ]; then
    echo -e "  ${GREEN}all green${RESET}"
    exit 0
else
    echo -e "  ${RED}$FAIL failed${RESET}"
    exit 1
fi
