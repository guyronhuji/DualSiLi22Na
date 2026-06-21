#!/usr/bin/env bash
set -euo pipefail

GEANT4_VERSION="${GEANT4_VERSION:-11.4.0}"
GEANT4_PREFIX="${GEANT4_PREFIX:-$HOME/Code/GEANT4}"
VENV_DIR="${VENV_DIR:-$HOME/dual-sili-venv}"
JOBS="${JOBS:-$(nproc)}"
BUILD_GEANT4_QT="${BUILD_GEANT4_QT:-OFF}"

sudo apt-get update
sudo apt-get install -y \
  build-essential \
  ca-certificates \
  cmake \
  curl \
  git \
  libexpat1-dev \
  libgl1-mesa-dev \
  libglu1-mesa-dev \
  libx11-dev \
  libxext-dev \
  libxmu-dev \
  ninja-build \
  python3 \
  python3-dev \
  python3-pip \
  python3-venv \
  zlib1g-dev

if [[ "$BUILD_GEANT4_QT" == "ON" ]]; then
  sudo apt-get install -y qt6-base-dev libxpm-dev
fi

python3 -m venv "$VENV_DIR"
source "$VENV_DIR/bin/activate"
python -m pip install --upgrade pip
python -m pip install jupyter matplotlib numpy pandas pyarrow scipy

if [[ -x "$GEANT4_PREFIX/bin/geant4-config" ]]; then
  echo "Using existing Geant4 at $GEANT4_PREFIX"
  "$GEANT4_PREFIX/bin/geant4-config" --version
  exit 0
fi

mkdir -p "$HOME/Code"
workdir="${TMPDIR:-/tmp}/geant4-${GEANT4_VERSION}-build"
srcdir="${TMPDIR:-/tmp}/geant4-v${GEANT4_VERSION}"
tarball="${TMPDIR:-/tmp}/geant4-v${GEANT4_VERSION}.tar.gz"

if [[ ! -d "$srcdir" ]]; then
  curl -L -o "$tarball" \
    "https://gitlab.cern.ch/geant4/geant4/-/archive/v${GEANT4_VERSION}/geant4-v${GEANT4_VERSION}.tar.gz"
  tar -C "${TMPDIR:-/tmp}" -xzf "$tarball"
fi

cmake -S "$srcdir" -B "$workdir" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$GEANT4_PREFIX" \
  -DGEANT4_BUILD_MULTITHREADED=ON \
  -DGEANT4_INSTALL_DATA=ON \
  -DGEANT4_USE_OPENGL_X11="$BUILD_GEANT4_QT" \
  -DGEANT4_USE_QT="$BUILD_GEANT4_QT" \
  -DGEANT4_USE_SYSTEM_EXPAT=ON \
  -DGEANT4_USE_SYSTEM_ZLIB=ON

cmake --build "$workdir" -j "$JOBS"
cmake --install "$workdir"

"$GEANT4_PREFIX/bin/geant4-config" --version

echo
echo "Geant4 installed at $GEANT4_PREFIX"
echo "Python environment installed at $VENV_DIR"
echo "Before building DualSiLi22Na, run:"
echo "  source $VENV_DIR/bin/activate"
echo "  source $GEANT4_PREFIX/bin/geant4.sh"
