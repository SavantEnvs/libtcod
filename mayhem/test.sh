#!/usr/bin/env bash
#
# mayhem/test.sh — runs libtcod's own Catch2 suite (tests/test_*.cpp, built by mayhem/build.sh with
# normal flags and SDL off) and reports its counts as CTRF. The counts are parsed from Catch2's own
# summary line, so a neutered or missing runner (empty output) fails instead of passing.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}


RUNNER="$SRC/build/tests/tests/unittest"
[ -x "$RUNNER" ] || RUNNER=$(find "$SRC/build/tests" -name unittest -type f -perm -u+x | head -n1)
if [ -z "$RUNNER" ] || [ ! -x "$RUNNER" ]; then
  echo "unittest runner missing: mayhem/build.sh should have built it" >&2
  emit_ctrf "catch2" 0 1 0
  exit 1
fi

# Tests that need an SDL window/context or the wall clock are excluded: SDL is compiled out of this build.
export DATA_DIR="$SRC/data/"
OUT=$("$RUNNER" --durations no 2>&1)
printf '%s\n' "$OUT" | tail -n 40

if printf '%s\n' "$OUT" | grep -q '^All tests passed'; then
  total=$(printf '%s\n' "$OUT" | sed -n 's/^All tests passed ([0-9]* assertions\? in \([0-9]*\) test case.*/\1/p' | tail -n1)
  [ -n "$total" ] || { emit_ctrf "catch2" 0 1 0; exit 1; }
  emit_ctrf "catch2" "$total" 0 0
else
  line=$(printf '%s\n' "$OUT" | grep -E '^test cases: ' | tail -n1)
  if [ -z "$line" ]; then
    echo "no Catch2 summary line in the runner output" >&2
    emit_ctrf "catch2" 0 1 0
    exit 1
  fi
  # "1 failed as expected" is an upstream [!shouldfail] test that behaves as designed: a pass.
  expected=$(printf '%s\n' "$line" | sed -n 's/.*| *\([0-9]*\) failed as expected.*/\1/p')
  line=${line//failed as expected/}
  passed=$(printf '%s\n' "$line" | sed -n 's/.*| *\([0-9]*\) passed.*/\1/p')
  failed=$(printf '%s\n' "$line" | sed -n 's/.*| *\([0-9]*\) failed.*/\1/p')
  skipped=$(printf '%s\n' "$line" | sed -n 's/.*| *\([0-9]*\) skipped.*/\1/p')
  emit_ctrf "catch2" $(( ${passed:-0} + ${expected:-0} )) "${failed:-0}" "${skipped:-0}"
fi
