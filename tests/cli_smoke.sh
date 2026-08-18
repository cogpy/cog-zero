#!/usr/bin/env bash
# cli_smoke.sh — smoke tests for near-term standalone CLI enhancements.
#
# Exercises:
#   --batch JSON status output
#   save / load AtomStore snapshot commands
#   rule registration + infer
#   --no-color (no ANSI escapes in output)
#   semicolon-separated --eval
#
# Usage: cli_smoke.sh <path-to-cog0-binary>
# Exit 0 on success, 1 on failure.

set -euo pipefail

COG0="${1:?usage: cli_smoke.sh <path-to-cog0>}"
if [[ ! -x "$COG0" ]]; then
    echo "FAIL: cog0 binary not executable: $COG0" >&2
    exit 1
fi

WORKDIR="$(mktemp -d "${TMPDIR:-/tmp}/cog0_cli_smoke.XXXXXX")"
cleanup() { rm -rf "$WORKDIR"; }
trap cleanup EXIT

pass=0
fail=0

check() {
    local name="$1"
    shift
    if "$@"; then
        echo "  PASS  $name"
        pass=$((pass + 1))
    else
        echo "  FAIL  $name" >&2
        fail=$((fail + 1))
    fi
}

echo "=== cog0 CLI Smoke Tests ==="
echo ""

# -----------------------------------------------------------------------
# 1. --batch emits a JSON status object (and nothing else on stdout)
# -----------------------------------------------------------------------
out="$WORKDIR/batch.json"
"$COG0" --batch --name smoke-agent --eval "goal explore Explore" >"$out" 2>"$WORKDIR/batch.err"
check "batch_json_status" grep -q '"status": "ok"' "$out"
check "batch_json_agent"  grep -q '"agent": "smoke-agent"' "$out"
check "batch_json_goals"  grep -q '"goals": \["explore"\]' "$out"
check "batch_stdout_is_json_only" bash -c "head -1 '$out' | grep -q '^{'"
# batch mode should not write INFO logs to stdout
check "batch_no_info_on_stdout" bash -c "! grep -q '\\[INFO' '$out'"

# -----------------------------------------------------------------------
# 2. save / load round-trip via --script
# -----------------------------------------------------------------------
snap="$WORKDIR/store.snap"
cat >"$WORKDIR/save.cog0" <<EOF
goal keep Keep state
percept hello world
run 1
save $snap
EOF
"$COG0" --batch --script "$WORKDIR/save.cog0" >"$WORKDIR/save.json" 2>/dev/null
check "save_creates_snapshot" test -s "$snap"
check "save_snapshot_header" grep -q 'cog0-atomstore-snapshot v1' "$snap"

cat >"$WORKDIR/load.cog0" <<EOF
load $snap
run 1
EOF
"$COG0" --batch --script "$WORKDIR/load.cog0" >"$WORKDIR/load.json" 2>/dev/null
# After load + 1 cycle the store must still hold atoms from the snapshot
atoms=$(grep -o '"atoms": [0-9]*' "$WORKDIR/load.json" | head -1 | awk '{print $2}')
check "load_restores_atoms" bash -c "test '${atoms:-0}' -ge 1"

# -----------------------------------------------------------------------
# 3. rule command + infer (rule fires when condition concept exists)
# -----------------------------------------------------------------------
cat >"$WORKDIR/rule.cog0" <<'EOF'
goal seed Seed knowledge
run 1
rule derive if-exists Goal:seed then-add DerivedConcept
infer
atoms
EOF
"$COG0" --batch --script "$WORKDIR/rule.cog0" >"$WORKDIR/rule.json" 2>/dev/null
check "rule_registered" grep -q '"rules": 4' "$WORKDIR/rule.json"

# Non-batch: assert the rule fired and DerivedConcept was added
"$COG0" --no-color --script "$WORKDIR/rule.cog0" >"$WORKDIR/rule_out.txt" 2>/dev/null
check "rule_infer_fires" grep -q 'fired  derive' "$WORKDIR/rule_out.txt"
check "rule_adds_derived_concept" grep -q 'DerivedConcept' "$WORKDIR/rule_out.txt"

# -----------------------------------------------------------------------
# 4. --no-color suppresses ANSI escape sequences
# -----------------------------------------------------------------------
"$COG0" --no-color --eval "help" >"$WORKDIR/nocolor.txt" 2>/dev/null
check "no_color_no_ansi" bash -c "! grep -q \$'\\033' '$WORKDIR/nocolor.txt'"

# -----------------------------------------------------------------------
# 5. semicolon-separated --eval
# -----------------------------------------------------------------------
"$COG0" --batch --eval "goal a First; goal b Second; run 1" >"$WORKDIR/multi.json" 2>/dev/null
check "eval_multi_goal_a" grep -q '"a"' "$WORKDIR/multi.json"
check "eval_multi_goal_b" grep -q '"b"' "$WORKDIR/multi.json"
check "eval_multi_cycles" grep -q '"cycles": 1' "$WORKDIR/multi.json"

# -----------------------------------------------------------------------
echo ""
echo "Results: $pass passed, $fail failed"
test "$fail" -eq 0
