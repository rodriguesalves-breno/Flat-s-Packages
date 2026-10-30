#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-only
# Test suite for flatspkg.
#
# Run from the repo root:
#   ./tests/test.sh
# or through make:
#   make check

set -u

BIN="${BIN:-./flatspkg}"
PASS=0
FAIL=0

pass() { printf 'PASS: %s\n' "$*"; PASS=$((PASS + 1)); }
fail() { printf 'FAIL: %s\n' "$*"; FAIL=$((FAIL + 1)); }

assert_exit() {
	local expected="$1" desc="$2"; shift 2
	local code=0
	"$@" >/dev/null 2>&1 || code=$?
	if [ "$code" -eq "$expected" ]; then
		pass "$desc"
	else
		fail "$desc (expected exit $expected, got $code)"
	fi
}

assert_output() {
	local desc="$1" expected="$2"; shift 2
	local out
	out=$(LANG=C LANGUAGE=C "$@" 2>&1)
	case "$out" in
		*"$expected"*) pass "$desc" ;;
		*) fail "$desc (expected '$expected', got '$out')" ;;
	esac
}

if [ ! -x "$BIN" ]; then
	printf 'ERROR: binary not found or not executable: %s\n' "$BIN" >&2
	exit 1
fi

# --- CLI basics ---
assert_exit 0 "binary --version returns 0" "$BIN" --version
assert_exit 0 "binary --help returns 0"    "$BIN" --help

assert_output "--version shows program name" "Flat's Packages" "$BIN" --version
assert_output "--help shows usage"           "Usage:"          "$BIN" --help

# Extract the expected version from flatspkg.c when available
SRC_VER=""
if [ -f flatspkg.c ]; then
	SRC_VER=$(sed -En 's/^#define[[:space:]]+VERSION[[:space:]]+"([^"]+)".*/\1/p' \
	          flatspkg.c | head -n1)
fi
if [ -n "$SRC_VER" ]; then
	assert_output "--version shows version ($SRC_VER)" \
	              "$SRC_VER" "$BIN" --version
else
	pass "--version shows version (skipped: flatspkg.c not found)"
fi

# --- i18n (auto-detection) ---
for entry in \
	"pt_BR.UTF-8:Uso:" \
	"es_ES.UTF-8:Uso:" \
	"fr_FR.UTF-8:Utilisation" \
	"de_DE.UTF-8:Verwendung"
do
	lang="${entry%%:*}"
	expected="${entry#*:}"
	out=$(LANG="$lang" LANGUAGE="$lang" "$BIN" --help 2>&1)
	case "$out" in
		*"$expected"*) pass "--help in $lang" ;;
		*) fail "--help in $lang (got '$out')" ;;
	esac
done

# LANGUAGE must win over LANG
out=$(LANGUAGE=pt_BR LANG=C "$BIN" --help 2>&1)
case "$out" in
	*"Uso:"*) pass "LANGUAGE overrides LANG" ;;
	*) fail "LANGUAGE overrides LANG (got '$out')" ;;
esac

# --- i18n (--lang flag) ---
out=$(LANGUAGE=C LANG=C "$BIN" --lang=pt_BR --help 2>&1)
case "$out" in
	*"Uso:"*) pass "--lang=pt_BR overrides env" ;;
	*) fail "--lang=pt_BR (got '$out')" ;;
esac

out=$(LANGUAGE=C LANG=C "$BIN" --lang=fr --help 2>&1)
case "$out" in
	*"Utilisation"*) pass "--lang=fr overrides env" ;;
	*) fail "--lang=fr (got '$out')" ;;
esac

out=$(LANGUAGE=pt_BR LANG=pt_BR.UTF-8 "$BIN" --lang=en --help 2>&1)
case "$out" in
	*"Usage:"*) pass "--lang=en forces English" ;;
	*) fail "--lang=en (got '$out')" ;;
esac

# Invalid language must fail
code=0
"$BIN" --lang=xx --help >/dev/null 2>&1 || code=$?
if [ "$code" -eq 1 ]; then
	pass "invalid --lang exits 1"
else
	fail "invalid --lang (expected exit 1, got $code)"
fi

# -l short form
out=$(LANGUAGE=C LANG=C "$BIN" -l de --help 2>&1)
case "$out" in
	*"Verwendung:"*) pass "-l de works" ;;
	*) fail "-l de (got '$out')" ;;
esac

# --- Robustness ---
code=0
env -u HOME "$BIN" --version >/dev/null 2>&1 || code=$?
if [ "$code" -eq 0 ]; then
	pass "--version works without HOME"
else
	fail "--version without HOME (exit $code)"
fi

# --- Dependency check ---
tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

code=0
PATH="$tmpdir" "$BIN" --update-cache >/dev/null 2>&1 || code=$?
if [ "$code" -eq 1 ]; then
	pass "missing flatpak yields exit 1"
else
	fail "missing flatpak (expected exit 1, got $code)"
fi

# --- Summary ---
printf '\n'
printf 'PASS: %d\n' "$PASS"
printf 'FAIL: %d\n' "$FAIL"
[ "$FAIL" -eq 0 ]