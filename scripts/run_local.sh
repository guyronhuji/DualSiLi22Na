#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$repo_root/build}"
JOBS="${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)}"
WEIGHTED="${WEIGHTED:-0}"

if [[ -n "${MACRO:-}" ]]; then
  macro="$MACRO"
elif [[ "$WEIGHTED" == "1" || "$WEIGHTED" == "ON" || "$WEIGHTED" == "true" ]]; then
  echo "WEIGHTED=1 is disabled: the old importance-sampling mode used a fast"
  echo "surrogate source and does not preserve the full 22Na decay/transport spectrum."
  echo "Run the unbiased macro instead, or pass MACRO=... explicitly for diagnostics."
  exit 2
else
  macro="$repo_root/macros/run_10000.mac"
fi

macro_for_parse=""
if [[ -f "$macro" ]]; then
  macro_for_parse="$macro"
elif [[ -f "$repo_root/$macro" ]]; then
  macro_for_parse="$repo_root/$macro"
elif [[ -f "$BUILD_DIR/$macro" ]]; then
  macro_for_parse="$BUILD_DIR/$macro"
fi

output_file=""
if [[ -n "$macro_for_parse" ]]; then
  output_file="$(awk '$1 == "/output/fileName" {print $2}' "$macro_for_parse" | tail -1)"
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
./DualSiLi22Na "$macro"
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
    read -r -a parquet_python_command <<< "${PARQUET_PYTHON_COMMAND:-python3}"
    "${parquet_python_command[@]}" "$combine_script" \
      --input-dir "$shard_dir" \
      --output "$expected_output"
  fi
fi

if [[ $run_status -ne 0 && ! -s "$expected_output" ]]; then
  exit "$run_status"
fi

echo
echo "Run complete. Output directory:"
echo "  $BUILD_DIR/output"
if [[ -n "$expected_output" ]]; then
  echo "Parquet file:"
  echo "  $expected_output"
fi
