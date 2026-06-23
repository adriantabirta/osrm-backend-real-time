#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

echo "=== Configuring ==="
cmake "${CMAKE_ARGS[@]}"

echo "=== Building (jobs=${JOBS}) ==="
cmake --build "${BUILD_DIR}" \
  --target osrm osrm-routed osrm-extract osrm-partition osrm-customize \
           live_traffic_publisher test_udp_packet \
  -j"${JOBS}"

echo ""
echo "Binaries:"
echo "  ${BUILD_DIR}/osrm-routed"
echo "  ${BUILD_DIR}/osrm-extract"
echo "  ${BUILD_DIR}/live_traffic_publisher"
echo ""
echo "Run server:"
echo "  ${BUILD_DIR}/osrm-routed ${DATASET} --enable-live-data --algorithm mld"
