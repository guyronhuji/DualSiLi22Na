#!/usr/bin/env python3
"""Validation plots for DualSiLi22Na event tables.

The script reads the default Parquet output, CSV shards, or legacy ROOT output
when uproot is available.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path

os.environ.setdefault("MPLCONFIGDIR", "/tmp/matplotlib")
os.environ.setdefault("XDG_CACHE_HOME", "/tmp")

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def load_table(path: Path) -> dict[str, np.ndarray]:
    if path.suffix.lower() == ".parquet":
        import pandas as pd

        data = pd.read_parquet(path)
        return {name: data[name].to_numpy() for name in data.columns}

    if path.suffix.lower() == ".csv":
        data = np.genfromtxt(path, delimiter=",", names=True)
        return {name: np.asarray(data[name]) for name in data.dtype.names}

    try:
        import uproot
    except ImportError as exc:
        raise SystemExit(
            "Reading ROOT output requires uproot. Install it or rerun with "
            "/output/fileName output/dual_sili_22na.parquet"
        ) from exc

    with uproot.open(path) as root_file:
        tree = root_file["DualSiLi22Na"]
        return tree.arrays(library="np")


def hist(ax, values, bins, title, xlabel):
    ax.hist(values, bins=bins, histtype="step", linewidth=1.3)
    ax.set_title(title)
    ax.set_xlabel(xlabel)
    ax.set_ylabel("Events")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "input",
        nargs="?",
        default="../output/dual_sili_22na.parquet",
        help="Parquet, CSV, or legacy ROOT output file",
    )
    parser.add_argument(
        "--outdir", default="validation_plots", help="Directory for PNG plots"
    )
    args = parser.parse_args()

    path = Path(args.input)
    outdir = Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)

    table = load_table(path)
    e1 = table["E_SiLi_1_keV"]
    e2 = table["E_SiLi_2_keV"]
    esum = table["E_SiLi_sum_keV"]
    hpge1 = table["E_HPGe_1_keV"]
    hpge2 = table["E_HPGe_2_keV"]
    hpge3 = table["E_HPGe_3_keV"]

    fig, axes = plt.subplots(2, 2, figsize=(11, 8))
    hist(axes[0, 0], e1, 200, "SiLi_1 spectrum", "Energy [keV]")
    hist(axes[0, 1], e2, 200, "SiLi_2 spectrum", "Energy [keV]")
    hist(axes[1, 0], esum, 200, "SiLi summed spectrum", "Energy [keV]")
    axes[1, 1].hist(e1, bins=200, histtype="step", label="SiLi_1")
    axes[1, 1].hist(e2, bins=200, histtype="step", label="SiLi_2")
    axes[1, 1].set_title("Si(Li) symmetry comparison")
    axes[1, 1].set_xlabel("Energy [keV]")
    axes[1, 1].set_ylabel("Events")
    axes[1, 1].legend()
    fig.tight_layout()
    fig.savefig(outdir / "sili_spectra.png", dpi=180)

    fig, axes = plt.subplots(1, 3, figsize=(14, 4))
    hist(axes[0], hpge1, 250, "HPGe_1 spectrum", "Energy [keV]")
    hist(axes[1], hpge2, 250, "HPGe_2 spectrum", "Energy [keV]")
    hist(axes[2], hpge3, 250, "HPGe_3 spectrum", "Energy [keV]")
    fig.tight_layout()
    fig.savefig(outdir / "hpge_spectra.png", dpi=180)

    fig, ax = plt.subplots(figsize=(6, 5))
    ax.hist2d(e1, e2, bins=160, range=[[0, 600], [0, 600]], cmap="viridis")
    ax.set_title("SiLi_1 vs SiLi_2")
    ax.set_xlabel("SiLi_1 energy [keV]")
    ax.set_ylabel("SiLi_2 energy [keV]")
    fig.tight_layout()
    fig.savefig(outdir / "sili1_vs_sili2.png", dpi=180)

    hpge_stack = np.maximum.reduce([hpge1, hpge2, hpge3])
    gate1274 = (hpge_stack > 1240.0) & (hpge_stack < 1310.0)
    gate_sili = esum > 0.0

    fig, axes = plt.subplots(1, 2, figsize=(11, 4))
    hist(
        axes[0],
        esum[gate1274],
        160,
        "Si(Li) sum gated by HPGe near 1274.5 keV",
        "Energy [keV]",
    )
    axes[1].hist(hpge1[gate_sili], bins=220, histtype="step", label="HPGe_1")
    axes[1].hist(hpge2[gate_sili], bins=220, histtype="step", label="HPGe_2")
    axes[1].hist(hpge3[gate_sili], bins=220, histtype="step", label="HPGe_3")
    axes[1].set_title("HPGe spectra gated by nonzero Si(Li)")
    axes[1].set_xlabel("Energy [keV]")
    axes[1].set_ylabel("Events")
    axes[1].legend()
    fig.tight_layout()
    fig.savefig(outdir / "gated_spectra.png", dpi=180)

    print(f"Wrote validation plots to {outdir.resolve()}")


if __name__ == "__main__":
    main()
