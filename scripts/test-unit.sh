#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

echo "=== 1/3 UDP wire format ==="
ensure_target test_udp_packet
"${BUILD_DIR}/test_udp_packet"

echo ""
echo "=== 2/3 Live traffic unit tests ==="
ensure_target engine-tests
"${BUILD_DIR}/unit_tests/engine-tests" --run_test=live_traffic

echo ""
echo "=== 3/3 Concurrency unit tests ==="
"${BUILD_DIR}/unit_tests/engine-tests" --run_test=live_traffic_concurrency

echo "OK: unit tests passed"
