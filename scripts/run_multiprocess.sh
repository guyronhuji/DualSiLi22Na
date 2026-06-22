#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$repo_root/build}"
MACRO="${MACRO:-$repo_root/macros/run_50000000.mac}"
PROCESSES="${PROCESSES:-$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)}"
THREADS_PER_PROCESS="${THREADS_PER_PROCESS:-1}"
JOBS="${JOBS:-$PROCESSES}"
BUILD="${BUILD:-1}"
RUN_LABEL="${RUN_LABEL:-$(date +%Y%m%d_%H%M%S)}"
PROGRESS="${PROGRESS:-1}"
PROGRESS_INTERVAL="${PROGRESS_INTERVAL:-5}"
PROGRESS_BAR_WIDTH="${PROGRESS_BAR_WIDTH:-40}"

if [[ "$MACRO" != /* ]]; then
  MACRO="$repo_root/$MACRO"
fi
if [[ ! -f "$MACRO" ]]; then
  echo "Macro not found: $MACRO" >&2
  exit 2
fi

total_events="${TOTAL_EVENTS:-}"
if [[ -z "$total_events" ]]; then
  total_events="$(awk '$1 == "/run/beamOn" {print $2}' "$MACRO" | tail -1)"
fi
if [[ -z "$total_events" || ! "$total_events" =~ ^[0-9]+$ ]]; then
  echo "Could not determine TOTAL_EVENTS. Set TOTAL_EVENTS=... or add /run/beamOn to the macro." >&2
  exit 2
fi

macro_output="$(awk '$1 == "/output/fileName" {print $2}' "$MACRO" | tail -1)"
if [[ -z "$macro_output" ]]; then
  echo "Could not determine output file. Add /output/fileName to the macro." >&2
  exit 2
fi

if [[ "$macro_output" == /* ]]; then
  default_output="$macro_output"
else
  default_output="$BUILD_DIR/$macro_output"
fi
final_output="${OUTPUT:-${default_output%.parquet}_mp.parquet}"
if [[ "$final_output" != /* ]]; then
  final_output="$BUILD_DIR/$final_output"
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

if [[ "$BUILD" == "1" || "$BUILD" == "ON" || "$BUILD" == "true" ]]; then
  cmake "${cmake_args[@]}"
  cmake --build "$BUILD_DIR" -j "$JOBS"
fi

run_dir="$BUILD_DIR/multiprocess_runs/$RUN_LABEL"
mkdir -p "$run_dir"

echo "Multiprocess run"
echo "  macro: $MACRO"
echo "  generated events: $total_events"
echo "  processes: $PROCESSES"
echo "  Geant4 threads per process: $THREADS_PER_PROCESS"
echo "  run directory: $run_dir"
echo "  final Parquet: $final_output"

base_events=$((total_events / PROCESSES))
extra_events=$((total_events % PROCESSES))
pids=()
part_outputs=()
part_offsets=()
part_logs=()
part_events=()
offset=0

cleanup() {
  if [[ ${#pids[@]} -gt 0 ]]; then
    kill "${pids[@]}" 2>/dev/null || true
  fi
}
trap cleanup INT TERM

for ((i = 0; i < PROCESSES; ++i)); do
  events_this="$base_events"
  if (( i < extra_events )); then
    events_this=$((events_this + 1))
  fi
  if (( events_this == 0 )); then
    continue
  fi

  part_macro="$run_dir/part_${i}.mac"
  part_output="$run_dir/part_${i}.parquet"
  part_log="$run_dir/part_${i}.log"
  part_outputs+=("$part_output")
  part_offsets+=("$offset")
  part_logs+=("$part_log")
  part_events+=("$events_this")

  awk \
    -v output="$part_output" \
    -v threads="$THREADS_PER_PROCESS" \
    -v events="$events_this" \
    '
      $1 == "/output/fileName" {
        print "/output/fileName " output
        next
      }
      $1 == "/run/numberOfThreads" {
        print "/run/numberOfThreads " threads
        next
      }
      $1 == "/run/beamOn" {
        print "/run/beamOn " events
        next
      }
      { print }
    ' "$MACRO" > "$part_macro"

  (
    cd "$BUILD_DIR"
    ./DualSiLi22Na "$part_macro" > "$part_log" 2>&1
  ) &
  pids+=("$!")

  echo "  launched part $i: events=$events_this offset=$offset log=$part_log"
  offset=$((offset + events_this))
done

latest_logged_event() {
  local log="$1"
  if [[ ! -f "$log" ]]; then
    echo 0
    return
  fi
  awk '
    {
      for (i = 1; i <= NF - 2; ++i) {
        if ($i == "Event" && $(i + 2) == "starts" && $(i + 1) ~ /^[0-9]+$/) {
          last = $(i + 1)
        }
      }
    }
    END {
      if (last == "") {
        print 0
      } else {
        print last
      }
    }
  ' "$log"
}

process_running() {
  local pid="$1"
  local running_pid
  while read -r running_pid; do
    if [[ "$running_pid" == "$pid" ]]; then
      return 0
    fi
  done < <(jobs -pr)
  return 1
}

print_progress() {
  local completed="$1"
  local active="$2"
  local percent=$((completed * 100 / total_events))
  local filled=$((completed * PROGRESS_BAR_WIDTH / total_events))
  local empty=$((PROGRESS_BAR_WIDTH - filled))
  local bar=""
  local i

  for ((i = 0; i < filled; ++i)); do
    bar+="#"
  done
  for ((i = 0; i < empty; ++i)); do
    bar+="-"
  done

  if [[ -t 1 ]]; then
    printf "\r[%s] %3d%% %s/%s events, %s active process(es)" \
      "$bar" "$percent" "$completed" "$total_events" "$active"
  else
    printf "[%s] %3d%% %s/%s events, %s active process(es)\n" \
      "$bar" "$percent" "$completed" "$total_events" "$active"
  fi
}

if [[ "$PROGRESS" == "1" || "$PROGRESS" == "ON" || "$PROGRESS" == "true" ]]; then
  while true; do
    completed=0
    active=0
    for ((i = 0; i < ${#pids[@]}; ++i)); do
      if process_running "${pids[$i]}"; then
        active=$((active + 1))
        latest="$(latest_logged_event "${part_logs[$i]}")"
        if (( latest > part_events[$i] )); then
          latest="${part_events[$i]}"
        fi
        completed=$((completed + latest))
      else
        completed=$((completed + part_events[$i]))
      fi
    done

    print_progress "$completed" "$active"
    if (( active == 0 )); then
      break
    fi
    sleep "$PROGRESS_INTERVAL"
  done
  if [[ -t 1 ]]; then
    printf "\n"
  fi
fi

failed=0
for pid in "${pids[@]}"; do
  if ! wait "$pid"; then
    failed=1
  fi
done
trap - INT TERM

if (( failed != 0 )); then
  echo "At least one worker process failed. Logs are in $run_dir" >&2
  exit 1
fi

read -r -a parquet_python_command <<< "${PARQUET_PYTHON_COMMAND:-python3}"
merge_args=("$final_output")
for ((i = 0; i < ${#part_outputs[@]}; ++i)); do
  merge_args+=("${part_outputs[$i]}" "${part_offsets[$i]}")
done

"${parquet_python_command[@]}" - "${merge_args[@]}" <<'PY'
from pathlib import Path
import sys

import pyarrow as pa
import pyarrow.compute as pc
import pyarrow.parquet as pq

output = Path(sys.argv[1])
items = sys.argv[2:]
if len(items) % 2:
    raise SystemExit("Expected parquet/offset pairs")

tables = []
for path_text, offset_text in zip(items[0::2], items[1::2]):
    path = Path(path_text)
    if not path.exists() or path.stat().st_size == 0:
        raise SystemExit(f"Missing worker Parquet: {path}")
    table = pq.read_table(path)
    offset = int(offset_text)
    event_id_index = table.schema.get_field_index("eventID")
    if event_id_index >= 0 and offset:
        event_ids = pc.add(
            table.column(event_id_index),
            pa.scalar(offset, type=table.schema.field(event_id_index).type),
        )
        table = table.set_column(event_id_index, "eventID", event_ids)
    tables.append(table)

output.parent.mkdir(parents=True, exist_ok=True)
merged = pa.concat_tables(tables, promote_options="default")
pq.write_table(merged, output, compression="zstd")
print(f"Merged {len(tables)} worker Parquet file(s), {len(merged)} event row(s)")
print(f"Wrote {output}")
PY

echo
echo "Multiprocess run complete:"
echo "  $final_output"
