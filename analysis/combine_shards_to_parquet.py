#!/usr/bin/env python3
"""Combine Geant4 worker CSV shards into one Parquet event table."""

from __future__ import annotations

import argparse
from pathlib import Path
import shutil

import pyarrow as pa
import pyarrow.compute as pc
import pyarrow.csv as csv
import pyarrow.parquet as pq


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", required=True, help="Directory of CSV shards")
    parser.add_argument("--output", required=True, help="Final Parquet output path")
    parser.add_argument(
        "--truth",
        action="store_true",
        help="Truth columns are expected. Kept for command readability.",
    )
    parser.add_argument(
        "--keep-shards",
        action="store_true",
        help="Keep worker CSV shards after writing Parquet",
    )
    parser.add_argument(
        "--sort",
        action="store_true",
        help="Sort rows by eventID before writing. Disabled by default for speed.",
    )
    args = parser.parse_args()

    input_dir = Path(args.input_dir)
    output = Path(args.output)
    shards = sorted(input_dir.glob("events_t*.csv"))
    if not shards:
        raise SystemExit(f"No CSV shards found in {input_dir}")

    tables = [csv.read_csv(path) for path in shards]
    table = pa.concat_tables(tables, promote_options="default")
    if args.sort:
        order = pc.sort_indices(table, sort_keys=[("eventID", "ascending")])
        table = pc.take(table, order)

    output.parent.mkdir(parents=True, exist_ok=True)
    pq.write_table(table, output, compression="zstd")

    print(f"Combined {len(shards)} shard(s), {len(table)} event row(s)")
    print(f"Wrote {output}")

    if not args.keep_shards:
        shutil.rmtree(input_dir)


if __name__ == "__main__":
    main()
