#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GEANT4_PREFIX="${GEANT4_PREFIX:-$HOME/Code/GEANT4}"
VENV_DIR="${VENV_DIR:-$HOME/dual-sili-venv}"
BUILD_DIR="${BUILD_DIR:-$repo_root/build-aws}"
JOBS="${JOBS:-$(nproc)}"
MACRO="${MACRO:-macros/run_50000000.mac}"

source "$VENV_DIR/bin/activate"
source "$GEANT4_PREFIX/bin/geant4.sh"

cmake -S "$repo_root" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DGeant4_DIR="$GEANT4_PREFIX/lib/cmake/Geant4" \
  -DDUALSILI_ENABLE_VIS=OFF

cmake --build "$BUILD_DIR" -j "$JOBS"

cd "$BUILD_DIR"
./DualSiLi22Na "$MACRO"

echo
echo "Final Parquet output:"
echo "  $BUILD_DIR/output/"
