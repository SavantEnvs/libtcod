#!/usr/bin/env bash
#
# mayhem/build.sh — builds libtcod (sanitized + SanCov, SDL off) and one fuzz binary plus one
# standalone reproducer per harness, then the project's Catch2 suite with normal flags.
set -euo pipefail

# clang rejects SOURCE_DATE_EPOCH='' (empty) — it must be unset or a valid integer.
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

# Build knobs come from the ENVIRONMENT (overridable), with sane defaults — no if-statements,
# just parameter-expansion fallbacks. Default sanitizers are the base's ASan+UBSan (halting); override
# per build via the Dockerfile's `--build-arg SANITIZER_FLAGS="..."`.
# NB: SANITIZER_FLAGS uses `=` (no colon) on purpose — `=` only fills when the var is UNSET, so an
# explicit EMPTY value (`--build-arg SANITIZER_FLAGS=`) is honored and builds with NO sanitizers
# (useful when you want the program's natural crash / full backtrace, not an ASan report). The other
# knobs use `:=` (default on empty too). MAYHEM_JOBS sets build parallelism (falls back to nproc).
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
# DEBUG_FLAGS carries DWARF debug info INDEPENDENTLY of the sanitizer off-switch (so an empty
# SANITIZER_FLAGS still yields DWARF symbols). DWARF MUST be < 4 (Mayhem triage can't read >=4); clang-19's
# plain `-g` emits DWARF-5, so `-gdwarf-3` is explicit. Apply $DEBUG_FLAGS to the fuzz/harness/standalone
# builds (NOT the test/oracle build). Rust/Go carry the same intent via their language flags below.
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${RUST_DEBUG_FLAGS:=-C debuginfo=2 -C force-frame-pointers=yes -C llvm-args=-gdwarf-3}"
: "${GO_DEBUG_FLAGS:=-gcflags=all=-N -l}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
# COVERAGE_FLAGS: empty by default → no effect on the normal oracle build. Set it via the Dockerfile's
# `--build-arg COVERAGE_FLAGS="-fprofile-instr-generate -fcoverage-mapping"` to instrument the TEST
# build for source-coverage measurement (how much of the project the test suite actually exercises —
# a quality signal for the oracle; complements the anti-reward-hack sabotage check). APPEND it to the
# test build's compile+link flags in step 3 (NOT the fuzz build); after `test.sh` runs, merge with
# `llvm-profdata` and report with `llvm-cov`. Empty value is honored (`=`, not `:=`).
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS RUST_DEBUG_FLAGS GO_DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"


# libtcod's CMake FetchContents utf8proc (empty submodule here) and Catch2; both come from the image:
# utf8proc is pre-built from the pinned commit, Catch2 is the apt package.
UTF8PROC_PREFIX=/opt/toolchains/deps/utf8proc-2.12.0
# console_rexpaint.c calls strerror() without <string.h>, which clang rejects as an implicit
# declaration; force-including the header fixes it without touching upstream.
CFIX="-include string.h"
CMAKE_COMMON=(-G Ninja -DFETCHCONTENT_FULLY_DISCONNECTED=ON "-DCMAKE_PREFIX_PATH=$UTF8PROC_PREFIX"
  -DLIBTCOD_SDL3=OFF -DLIBTCOD_ZLIB=ON -DLIBTCOD_LODEPNG=vendored -DLIBTCOD_STB=vendored
  -DLIBTCOD_UTF8PROC=ON -DLIBTCOD_INSTALL=OFF -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=None)

# 1) The oracle: upstream's unit tests with the project's normal flags (no sanitizer, no -gdwarf-3).
cmake -S "$SRC" -B "$SRC/build/tests" "${CMAKE_COMMON[@]}" -DLIBTCOD_TESTS=ON \
  -DCMAKE_C_FLAGS="$CFIX -O1 $COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="-O1 $COVERAGE_FLAGS"
cmake --build "$SRC/build/tests" -j"$MAYHEM_JOBS" --target unittest

# 2) The library, instrumented for the fuzzers: sanitizers, DWARF 3, and SanCov on the library itself
#    (-fsanitize=fuzzer-no-link, unconditionally, so an empty $SANITIZER_FLAGS still records edges).
#    UB checks kept halting.
LIBFLAGS="$SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS -O1"
cmake -S "$SRC" -B "$SRC/build/fuzz" "${CMAKE_COMMON[@]}" -DLIBTCOD_TESTS=OFF \
  -DCMAKE_C_FLAGS="$CFIX $LIBFLAGS" -DCMAKE_CXX_FLAGS="$LIBFLAGS"
cmake --build "$SRC/build/fuzz" -j"$MAYHEM_JOBS" --target libtcod

LIBTCOD_A=$(find "$SRC/build/fuzz" -maxdepth 1 -name 'libtcod*.a' | head -n1)
[ -n "$LIBTCOD_A" ] || { echo "libtcod static library not found" >&2; exit 1; }
UTF8PROC_A=$(find "$UTF8PROC_PREFIX" -name 'libutf8proc.a' | head -n1)
LIBS=("$LIBTCOD_A" "$UTF8PROC_A" -lz -lm -lpthread)
HFLAGS="$SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS -O1 -DLIBTCOD_STATIC -I$SRC/src"

# 3) Harnesses: a libFuzzer binary and a run-once standalone reproducer for each. The library mixes C
#    and C++, so the final link is done by $CXX. LSan is switched off at build time by lsan_off.cc.
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o /tmp/lsan_off.o
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o /tmp/standalone_main.o

for h in fuzz_fov fuzz_bdf fuzz_xp fuzz_png_tileset fuzz_print fuzz_asc fuzz_zip; do
  $CC $HFLAGS -c "$SRC/mayhem/harnesses/$h.c" -o "/tmp/$h.o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "/tmp/$h.o" /tmp/lsan_off.o "${LIBS[@]}" -o "/mayhem/$h"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "/tmp/$h.o" /tmp/standalone_main.o /tmp/lsan_off.o "${LIBS[@]}" -o "/mayhem/$h-standalone"
done

# apf_file is a file-input program (TCOD_console_load_apf leaks its FILE* on every error path, so an
# in-process target would run out of descriptors): no fuzz engine, but SanCov from -fsanitize=fuzzer-no-link.
$CC $HFLAGS -c "$SRC/mayhem/harnesses/apf_file.c" -o /tmp/apf_file.o
$CXX $SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS /tmp/apf_file.o /tmp/lsan_off.o "${LIBS[@]}" -o /mayhem/apf_file
