#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

export ROUTE_COORDS="${ROUTE_COORDS:-${DEFAULT_ROUTE_COORDS}}"
export DURATION_SEC="${DURATION_SEC:-15}"
export OSRM_URL
export UDP_PORT

if ! osrm_is_running; then
  echo "ERROR: no OSRM server at ${OSRM_URL}" >&2
  echo "  Run: ./scripts/start-osrm-server.sh" >&2
  echo "  Or:  ./scripts/run-all-tests.sh" >&2
  exit 1
fi

python3 "${ROOT}/scripts/test-stress.py"
echo "OK: stress test passed"
