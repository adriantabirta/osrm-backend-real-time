#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

PBF="${DATA_DIR}/moldova-latest.osm.pbf"
OSRM="${DATASET}"
URL="https://download.geofabrik.de/europe/moldova-latest.osm.pbf"

mkdir -p "${DATA_DIR}"

if [[ ! -f "${PBF}" ]]; then
  echo "=== Downloading Moldova OSM extract ==="
  if command -v wget >/dev/null 2>&1; then
    wget -O "${PBF}" "${URL}"
  elif command -v curl >/dev/null 2>&1; then
    curl -fsSL -o "${PBF}" "${URL}"
  else
    echo "ERROR: install wget or curl" >&2
    exit 1
  fi
else
  echo "=== PBF already present: ${PBF} ==="
fi

for bin in osrm-extract osrm-partition osrm-customize; do
  ensure_target "${bin}"
done

if [[ -f "${OSRM}.mldgr" ]]; then
  echo "=== Dataset already built: ${OSRM} ==="
else
  echo "=== Building MLD dataset (extract → partition → customize) ==="
  "${BUILD_DIR}/osrm-extract" -p "${ROOT}/profiles/car.lua" "${PBF}"
  EXTRACTED="${DATA_DIR}/moldova-latest.osrm"
  if [[ ! -f "${EXTRACTED}.properties" ]]; then
    echo "ERROR: expected ${EXTRACTED}.properties after extract" >&2
    exit 1
  fi
  "${BUILD_DIR}/osrm-partition" "${EXTRACTED}"
  "${BUILD_DIR}/osrm-customize" "${EXTRACTED}"
fi

ln -sfn moldova-latest.osrm "${DATA_DIR}/map.osrm"
echo "OK: ready at ${OSRM}.* (symlink data/map.osrm)"
