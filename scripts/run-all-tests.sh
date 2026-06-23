#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

STARTED_SERVER=0
cleanup() {
  if [[ "${STARTED_SERVER}" -eq 1 ]]; then
    "${ROOT}/scripts/stop-osrm-server.sh" || true
  fi
}
trap cleanup EXIT

"${ROOT}/scripts/test-unit.sh"

if ! osrm_is_running; then
  "${ROOT}/scripts/start-osrm-server.sh"
  STARTED_SERVER=1
fi

export DURATION_SEC="${DURATION_SEC:-15}"
"${ROOT}/scripts/test-stress.sh"
