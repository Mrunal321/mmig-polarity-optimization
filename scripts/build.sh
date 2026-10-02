#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
mkdir -p build .deps
if [[ "${MMIG_IN_DOCKER:-0}" == 1 ]]; then
  CMAKE_DIR=build/cmake_docker
else
  CMAKE_DIR=build/cmake_host
fi

ABC_COMMIT=ee899284b854bbd37acc9ddbe1dc598c9a2cf151
if [[ -n "${MMIG_ABC:-}" ]]; then
  ABC_BIN="$MMIG_ABC"
  echo "Using caller-provided ABC: $ABC_BIN"
else
  if [[ ! -d .deps/abc/.git ]]; then
    git clone https://github.com/berkeley-abc/abc.git .deps/abc
  fi
  git -C .deps/abc fetch --depth 1 origin "$ABC_COMMIT"
  git -C .deps/abc checkout --detach "$ABC_COMMIT"
  [[ "$(git -C .deps/abc rev-parse HEAD)" == "$ABC_COMMIT" ]]
  make -C .deps/abc -j "${MMIG_JOBS:-4}" ABC_USE_NO_READLINE=1
  ABC_BIN="$ROOT/.deps/abc/abc"
fi
[[ -x "$ABC_BIN" ]] || { echo "ABC binary unavailable: $ABC_BIN" >&2; exit 1; }

cmake -S . -B "$CMAKE_DIR" -DMOCKTURTLE_BUILD_EXAMPLES=OFF \
  -DMOCKTURTLE_BUILD_TESTS=OFF -DMOCKTURTLE_ENABLE_NAUTY=OFF
cmake --build "$CMAKE_DIR" --target libabcsat libabcesop -j "${MMIG_JOBS:-4}"

FLAGS=(-std=c++17 -O2 -DNDEBUG -DFMT_HEADER_ONLY -DABC_NAMESPACE=pabc
  -DABC_NO_USE_READLINE -DDISABLE_NAUTY -DLIN64)
for directory in include lib/abcsat lib/abcesop lib/kitty lib/lorina \
  lib/rang lib/fmt lib/parallel_hashmap lib/percy lib/json lib/bill; do
  FLAGS+=("-I$directory")
done
LIBS=("$CMAKE_DIR/lib/abcsat/liblibabcsat.a" "$CMAKE_DIR/lib/abcesop/liblibabcesop.a")
g++ "${FLAGS[@]}" examples/blif2mig_2.cpp "${LIBS[@]}" \
  -o build/blif2mig_paper > build/blif2mig_compile.log 2>&1
g++ "${FLAGS[@]}" src/phase_audit.cpp "${LIBS[@]}" \
  -o build/phase_audit > build/phase_audit_compile.log 2>&1
printf '%s\n' "$ABC_BIN" > build/abc_path.txt
sha256sum build/blif2mig_paper build/phase_audit "$ABC_BIN" \
  | tee build/binary_hashes.txt
