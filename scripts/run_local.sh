#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$repo_root/build}"
JOBS="${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)}"
WEIGHTED="${WEIGHTED:-0}"

if [[ -n "${MACRO:-}" ]]; then
  macro="$MACRO"
elif [[ "$WEIGHTED" == "1" || "$WEIGHTED" == "ON" || "$WEIGHTED" == "true" ]]; then
  macro="$repo_root/macros/run_importance_hpge_triple.mac"
else
  macro="$repo_root/macros/run_10000.mac"
fi

cmake_args=(
  -S "$repo_root"
  -B "$BUILD_DIR"
  -DCMAKE_BUILD_TYPE=Release
  -DDUALSILI_ENABLE_VIS=OFF
)

if [[ -n "${Geant4_DIR:-}" ]]; then
  cmake_args+=("-DGeant4_DIR=$Geant4_DIR")
elif [[ -d "$HOME/Code/GEANT4/lib/cmake/Geant4" ]]; then
  cmake_args+=("-DGeant4_DIR=$HOME/Code/GEANT4/lib/cmake/Geant4")
fi

if [[ "$(uname -s)" == "Darwin" ]]; then
  cmake_args+=("-DCMAKE_OSX_ARCHITECTURES=arm64")
fi

cmake "${cmake_args[@]}"
cmake --build "$BUILD_DIR" -j "$JOBS"

cd "$BUILD_DIR"
./DualSiLi22Na "$macro"

echo
echo "Run complete. Output directory:"
echo "  $BUILD_DIR/output"
