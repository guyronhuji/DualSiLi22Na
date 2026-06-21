#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GEANT4_PREFIX="${GEANT4_PREFIX:-$HOME/Code/GEANT4}"
VENV_DIR="${VENV_DIR:-$HOME/dual-sili-venv}"
BUILD_DIR="${BUILD_DIR:-$repo_root/build-aws}"
JOBS="${JOBS:-$(nproc)}"
MACRO="${MACRO:-macros/run_50000000.mac}"

macro_for_parse=""
if [[ -f "$MACRO" ]]; then
  macro_for_parse="$MACRO"
elif [[ -f "$repo_root/$MACRO" ]]; then
  macro_for_parse="$repo_root/$MACRO"
elif [[ -f "$BUILD_DIR/$MACRO" ]]; then
  macro_for_parse="$BUILD_DIR/$MACRO"
fi

output_file=""
if [[ -n "$macro_for_parse" ]]; then
  output_file="$(awk '$1 == "/output/fileName" {print $2}' "$macro_for_parse" | tail -1)"
fi

source "$VENV_DIR/bin/activate"
source "$GEANT4_PREFIX/bin/geant4.sh"

cmake -S "$repo_root" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DGeant4_DIR="$GEANT4_PREFIX/lib/cmake/Geant4" \
  -DDUALSILI_ENABLE_VIS=OFF

cmake --build "$BUILD_DIR" -j "$JOBS"

cd "$BUILD_DIR"
expected_output=""
if [[ -n "$output_file" ]]; then
  expected_output="$output_file"
  if [[ "$expected_output" != /* ]]; then
    expected_output="$BUILD_DIR/$expected_output"
  fi
  echo "Expected Parquet output:"
  echo "  $expected_output"
fi

set +e
./DualSiLi22Na "$MACRO"
run_status=$?
set -e

if [[ -n "$expected_output" && ! -s "$expected_output" ]]; then
  shard_dir="${expected_output%.*}_shards"
  if [[ -d "$shard_dir" ]]; then
    echo
    echo "Parquet output was not found; attempting recovery from shards:"
    echo "  $shard_dir"
    combine_script="$BUILD_DIR/analysis/combine_shards_to_parquet.py"
    if [[ ! -f "$combine_script" ]]; then
      combine_script="$repo_root/analysis/combine_shards_to_parquet.py"
    fi
    python "$combine_script" \
      --input-dir "$shard_dir" \
      --output "$expected_output"
  fi
fi

if [[ $run_status -ne 0 && ! -s "$expected_output" ]]; then
  exit "$run_status"
fi

echo
echo "Final Parquet output:"
if [[ -n "$expected_output" ]]; then
  echo "  $expected_output"
else
  echo "  $BUILD_DIR/output/"
fi
