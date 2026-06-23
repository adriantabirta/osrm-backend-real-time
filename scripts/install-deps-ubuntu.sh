#!/usr/bin/env bash
set -euo pipefail

echo "Installing OSRM live-traffic build dependencies (Ubuntu/Debian)..."

sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  build-essential \
  ca-certificates \
  cmake \
  git \
  pkg-config \
  libbz2-dev \
  libexpat1-dev \
  liblua5.4-dev \
  libtbb-dev \
  libzip-dev \
  libboost-all-dev \
  zlib1g-dev \
  python3

echo "Done."
