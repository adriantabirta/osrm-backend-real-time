#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

if osrm_is_running; then
  echo "OSRM already running at ${OSRM_URL}"
  exit 0
fi

if [[ ! -f "${DATASET}.mldgr" && ! -f "${DATASET}.properties" ]]; then
  echo "Dataset missing — preparing Moldova data..."
  "${ROOT}/scripts/prepare-moldova-data.sh"
fi

ensure_target osrm-routed

echo "Starting osrm-routed with live data on port ${OSRM_PORT}..."
"${BUILD_DIR}/osrm-routed" "${DATASET}" \
  --enable-live-data \
  --port "${OSRM_PORT}" \
  --live-data-udp-port "${UDP_PORT}" \
  --algorithm mld \
  >/tmp/osrm-routed.log 2>&1 &
echo $! > "${PID_FILE}"

for _ in $(seq 1 60); do
  if osrm_is_running; then
    echo "OSRM ready at ${OSRM_URL} (pid $(cat "${PID_FILE}"))"
    exit 0
  fi
  if ! kill -0 "$(cat "${PID_FILE}")" 2>/dev/null; then
    echo "ERROR: osrm-routed exited. Log:" >&2
    tail -30 /tmp/osrm-routed.log >&2 || true
    exit 1
  fi
  sleep 1
done

echo "ERROR: osrm-routed failed to start. Log:" >&2
tail -30 /tmp/osrm-routed.log >&2 || true
exit 1
