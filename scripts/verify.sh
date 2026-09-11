#!/usr/bin/env bash
# The single verification entry point: close a running app, configure if
# needed, build every target in the preset, fail on any warning from our own
# code, then run ctest. Terse on success, detailed only on failure.
#
# Usage: scripts/verify.sh [--reconfigure] [--full] [--no-tests]
#   --reconfigure  force the configure step (implied by a missing cache)
#   --full         wipe our targets' object dirs first; moc output only warns on
#                  a full rebuild, never on an incremental one
#   --no-tests     build and warning gate only
set -uo pipefail

cd "$(dirname "$0")/.." || exit 1

# The cmake on PATH is Strawberry Perl's 3.29: too old for this MSVC, and it
# overwrites CMakeCache.txt before failing. Always the full path.
CMAKE=${VERIFY_CMAKE:-C:/Qt/Tools/CMake_64/bin/cmake.exe}
CTEST=${VERIFY_CTEST:-C:/Qt/Tools/CMake_64/bin/ctest.exe}
PRESET=msvc-debug
BUILD_DIR=build/$PRESET
LOG=$BUILD_DIR/verify.log

reconfigure=0
full=0
run_tests=1
for arg in "$@"; do
    case "$arg" in
        --reconfigure) reconfigure=1 ;;
        --full) full=1 ;;
        --no-tests) run_tests=0 ;;
        *) echo "verify: unknown option $arg" >&2; exit 2 ;;
    esac
done

fail() { echo "[FAIL] $*"; exit 1; }
step() { printf '[ok] %-12s %ss\n' "$1" "$2"; }

# ------------------------------------------------------------- 1. close the app
# A running MegaDirStat.exe holds its own .exe open and the link dies LNK1104.
# No `tasklist | grep`: under pipefail a SIGPIPE'd tasklist reads as "not running".
running() {
    local out
    out=$(tasklist //FI "IMAGENAME eq $1" 2>/dev/null) || return 1
    [[ ${out,,} == *"${1,,}"* ]]
}
if running MegaDirStat.exe; then
    taskkill //IM MegaDirStat.exe //F >/dev/null 2>&1
    for _ in 1 2 3 4 5 6 7 8 9 10; do
        running MegaDirStat.exe || break
        sleep 1
    done
    running MegaDirStat.exe && fail "MegaDirStat.exe could not be closed; the link would hit LNK1104"
    echo "[ok] closed running MegaDirStat.exe"
fi

# --------------------------------------------------------------- 2. full reset
if [ "$full" -eq 1 ] && [ -d "$BUILD_DIR" ]; then
    rm -rf "$BUILD_DIR"/MegaDirStat*.dir "$BUILD_DIR"/tests/*.dir \
        "$BUILD_DIR"/*_autogen "$BUILD_DIR"/tests/*_autogen
    echo "[ok] wiped our object dirs (--full)"
fi

# ------------------------------------------------------------- 3. (re)configure
mkdir -p "$BUILD_DIR"
if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    reconfigure=1
fi
if [ "$reconfigure" -eq 1 ]; then
    SECONDS=0
    if ! "$CMAKE" --preset "$PRESET" >"$LOG" 2>&1; then
        echo "--- configure output (tail) ---"
        tail -n 30 "$LOG"
        fail "configure"
    fi
    step configure "$SECONDS"
fi

# -------------------------------------------------------------------- 4. build
SECONDS=0
"$CMAKE" --build --preset "$PRESET" >"$LOG" 2>&1
build_status=$?
build_time=$SECONDS

# Ours only; third_party is not held to our warning level. Sorted unique
# because MSBuild repeats each diagnostic in its end-of-build summary.
warnings=$(grep -E 'warning [A-Z]+[0-9]+' "$LOG" | grep -v 'third_party' | sed 's/^ *//' | sort -u)

if [ "$build_status" -ne 0 ]; then
    echo "--- build errors ---"
    grep -E 'error [A-Z]+[0-9]+|LNK[0-9]{4}' "$LOG" | grep -v 'third_party' | sed 's/^ *//' | sort -u | head -n 40
    fail "build (${build_time}s)"
fi
step build "$build_time"

if [ -n "$warnings" ]; then
    echo "--- warnings (must be zero) ---"
    printf '%s\n' "$warnings" | head -n 40
    fail "warning gate: $(printf '%s\n' "$warnings" | wc -l) warning(s)"
fi
echo "[ok] warnings     0"

# -------------------------------------------------------------------- 5. tests
if [ "$run_tests" -eq 1 ]; then
    SECONDS=0
    if ! "$CTEST" --preset "$PRESET" >"$LOG" 2>&1; then
        echo "--- failing tests ---"
        grep -E '^\s*[0-9]+ - .*(Failed|Timeout)|tests failed out of|FAIL!' "$LOG" | head -n 40
        fail "ctest (${SECONDS}s)"
    fi
    step ctest "$SECONDS"
    grep -E 'tests passed' "$LOG" | tail -n 1
fi

echo "[PASS] verify"
