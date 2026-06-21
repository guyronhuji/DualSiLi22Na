# DualSiLi22Na

Standalone Geant4 simulation of a thin central `22Na` source between two
opposing cryogenic Si(Li) detectors, with three coaxial HPGe gamma detectors.

## Prerequisites

- Geant4 with UI and visualization support.
- CMake 3.16 or newer.
- A C++17 compiler.
- Python with `numpy`, `matplotlib`, `pandas`, and `pyarrow` for Parquet output
  and validation plots.

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

Fresh single-config builds default to `Release`. For an existing build
directory, reconfigure once to replace an empty or debug build type:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

If your Geant4 installation needs environment setup, source its `geant4.sh`
before configuring.

On Apple Silicon, a native arm64 Geant4 installation under `~/Code/GEANT4` or
`/opt/homebrew` is preferred automatically when present. If only `/usr/local`
Geant4 is installed, the executable must be built for that installation's
architecture.

### Qt Visualization Build

Keep the high-statistics batch executable lean, and use a separate Qt build
directory for interactive geometry/event display:

```bash
cmake -S . -B build-qt \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DGeant4_DIR="$HOME/Code/GEANT4/lib/cmake/Geant4" \
  -DDUALSILI_ENABLE_QT=ON \
  -DQt6_DIR=/opt/homebrew/opt/qtbase/lib/cmake/Qt6 \
  -DQt6Core_DIR=/opt/homebrew/opt/qtbase/lib/cmake/Qt6Core \
  -DQt6Gui_DIR=/opt/homebrew/opt/qtbase/lib/cmake/Qt6Gui \
  -DQt6Widgets_DIR=/opt/homebrew/opt/qtbase/lib/cmake/Qt6Widgets \
  -DQt6OpenGL_DIR=/opt/homebrew/opt/qtbase/lib/cmake/Qt6OpenGL \
  -DQt6OpenGLWidgets_DIR=/opt/homebrew/opt/qtbase/lib/cmake/Qt6OpenGLWidgets
cmake --build build-qt -j
```

Launch the Qt viewer with:

```bash
./build-qt/DualSiLi22Na
```

When no macro is supplied, the Qt executable loads `macros/vis_qt.mac`, opens
the `TSGQt` viewer, draws the detector volumes, shows axes at the source, and
accumulates the first 20 trajectories. Supplying a macro argument still runs in
batch mode:

```bash
./build-qt/DualSiLi22Na build-qt/macros/run_10000.mac
```

On macOS, if a prebuilt Geant4 package references an old SDK path for EXPAT,
configure with the active SDK paths, for example:

```bash
SDK="$(xcrun --show-sdk-path)"
cmake -S . -B build \
  -DCMAKE_OSX_SYSROOT="$SDK" \
  -DEXPAT_LIBRARY="$SDK/usr/lib/libexpat.tbd" \
  -DEXPAT_LIBRARY_RELEASE="$SDK/usr/lib/libexpat.tbd" \
  -DEXPAT_INCLUDE_DIR="$SDK/usr/include"
cmake --build build -j
```

## Run

Run the default 10,000-event validation macro:

```bash
./build/DualSiLi22Na build/macros/run_10000.mac
```

Or use the local helper, which configures/builds and runs a macro:

```bash
bash scripts/run_local.sh
```

The local helper prints the exact expected Parquet path before the run starts.

During a run, Geant4 writes worker CSV shards first. The final Parquet file is
created at end-of-run. If the executable exits after writing shards but before
creating Parquet, the helper attempts to recover the Parquet from those shards.

You can also choose any macro explicitly:

```bash
MACRO=macros/run_importance_hpge_triple.mac bash scripts/run_local.sh
```

The macro uses four Geant4 worker threads by default:

```text
/run/numberOfThreads 4
```

The default output file is:

```text
build/output/dual_sili_22na.parquet
```

For a 50,000,000-event EC2 run, use the AWS bootstrap and production scripts in
`aws/`. The production macro uses 16 Geant4 worker threads and prints progress
every 10,000 events:

```bash
bash aws/bootstrap_ubuntu_24_04.sh
bash aws/run_production_ubuntu.sh
```

The previous importance-sampled HPGe triple-coincidence mode is disabled. It
used a hand-made fast `22Na` surrogate source, emitted annihilation photons at
the source, and suppressed Geant4 positron annihilation. That made weighted
outputs faster, but it did not preserve the full decay/transport spectrum and
should not be used for production physics.

During a multithreaded run, each worker writes a private CSV shard under:

```text
output/dual_sili_22na_shards/
```

At end-of-run the master process combines those shards into the requested
Parquet file with `analysis/combine_shards_to_parquet.py` and removes the shard
directory.

On Apple Silicon with an x86_64 Geant4 build, the default Parquet combiner uses:

```text
/output/parquetPythonCommand /usr/bin/arch -arm64 python3
```

Change that command if your Python/pyarrow installation lives elsewhere.

## View Geometry

Start interactive visualization:

```bash
./build/DualSiLi22Na
```

The executable loads `macros/vis.mac` when no macro argument is supplied.
For the Qt interface, use the `build-qt` executable described above.

To run the Geant4 overlap test directly:

```text
/run/initialize
/geometry/test/run
```

## Detector Coordinates

| Detector | Position convention | Front face |
| --- | --- | --- |
| Source | Centered at `(0, 0, 0)` | Disk axis along `Z` |
| SiLi_1 | `+Z` side | Faces `-Z`, toward source |
| SiLi_2 | `-Z` side | Faces `+Z`, toward source |
| HPGe_1 | `+X` side | Front face 33 mm from source |
| HPGe_2 | `-X` side | Front face 33 mm from source |
| HPGe_3 | `+Y` side | Front face 33 mm from source |

The HPGe front-face distance is measured from the source center to the thin
aluminum entrance window plane, not to the crystal center. The close geometry
uses 33 mm because each HPGe assembly has a 32.7 mm outer housing radius; this
puts the two 90-degree HPGe assemblies just outside mutual contact while keeping
the front faces as close as practical to the source and the Si(Li) stack.

Only `SiLi_1`, `SiLi_2`, `HPGe_1`, `HPGe_2`, and `HPGe_3` active volumes
score deposited energy. Housings, windows, source, Kapton, and dead layers are
not detector scoring volumes.

## Physics

The custom modular physics list registers:

- `G4EmStandardPhysics` for faster production electromagnetic transport;
- `G4DecayPhysics` for unstable particle decays;
- `G4RadioactiveDecayPhysics` for ion radioactive decay.

Atomic de-excitation is disabled in the world region by default for speed.
Use a higher-accuracy EM list and re-enable fluorescence/Auger/PIXE if
low-energy atomic-relaxation details become important.

The primary generator creates a `22Na` ion at rest, uniformly distributed in
the thin source disk. It does not manually generate positrons, 511 keV photons,
or the 1274.5 keV gamma.

The source time defaults to `1e20 s`. This is intentionally far beyond the
`22Na` lifetime so radioactive decay timing does not suppress decays in finite
event runs. Change it with:

```text
/source/decayTime 1e20 s
```

Do not use a short source time for normal `22Na` runs.

The physics list also raises Geant4's radioactive-decay time threshold to
`1e30 s`; otherwise long-lived decays scheduled near `1e20 s` can be ignored.

## Output Fields

The final Parquet table contains one row per event:

- `eventID`
- `E_SiLi_1_keV`
- `E_SiLi_2_keV`
- `E_SiLi_sum_keV`
- `E_HPGe_1_keV`
- `E_HPGe_2_keV`
- `E_HPGe_3_keV`
- `E_total_all_detectors_keV`
- hit counts for each detector
- first hit time for each detector in ns

Optional truth fields are enabled with:

```text
/output/truthOutput true
```

They include the primary vertex/time and simple secondary counters for
positrons, 511 keV gammas, and 1274.5 keV gammas.

## Validation Plots

From the build directory:

```bash
python3 analysis/plot_validation.py output/dual_sili_22na.parquet
```

The script writes:

- individual `SiLi_1` and `SiLi_2` spectra;
- summed Si(Li) spectrum;
- Si(Li) symmetry comparison;
- HPGe spectra;
- `E_SiLi_1` vs `E_SiLi_2`;
- gated spectra near the HPGe 1274.5 keV response and nonzero Si(Li) energy.

Expected qualitative features:

- `SiLi_1` and `SiLi_2` agree statistically;
- the summed Si(Li) spectrum recovers some split/backscatter events;
- HPGe spectra show response near 511 keV and 1274.5 keV;
- `HPGe_1` and `HPGe_2` are symmetric;
- `HPGe_3` is the 90-degree detector.

## Spectrum Analysis Notebook

An exploratory notebook with spectra, coincidence plots, Gaussian peak fits,
Compton-edge estimates, and Si(Li) symmetry checks is available at:

```text
analysis/notebooks/dual_sili_22na_spectrum_analysis.ipynb
```

It reads `output/dual_sili_22na.parquet` and writes regenerated figures under
`output/notebook_figures/`.

If the input has `event_weight`, notebook histograms and gates use those
weights. The standard production path is the unbiased radioactive-decay run.

## Geometry Controls

Macro commands:

```text
/run/numberOfThreads 4
/geometry/sourceRadius 2.5 mm
/geometry/sourceThickness 10 um
/geometry/kaptonThickness 1 um
/geometry/siliActiveDiameter 50 mm
/geometry/siliActiveThickness 5 mm
/geometry/siliCrystalFaceGap 2 mm
/geometry/hpgeFrontDistance 33 mm
/source/decayTime 1e20 s
/source/fast22NaPrimaries false
/source/importanceSampling false
/source/importanceMode hpgeTriple
/source/importanceConeHalfAngle 45 deg
/source/betaEndpointEnergy 545 keV
/source/suppressFastPositronAnnihilation true
/source/positronKillEnergy 0.1 keV
/output/fileName output/dual_sili_22na.parquet
/output/parquetPythonCommand /usr/bin/arch -arm64 python3
/output/truthOutput false
```

Major defaults live in `include/DetectorParameters.hh`.

## Known Simplifications

- No electronic resolution smearing is applied by default.
- The default EM physics is tuned for production speed rather than maximum
  low-energy atomic-detail fidelity.
- The source material is a low-density sodium-chloride surrogate.
- Detector geometry is idealized until refined mechanical drawings are used.
- Truth counters are practical validation counters based on secondary tracks,
  not a full decay-chain ancestry record.
