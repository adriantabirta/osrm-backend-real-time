#!/usr/bin/env bash
set -euo pipefail

LIVE_DATA_UDP_PORT="${LIVE_DATA_UDP_PORT:-9900}"
LIVE_DATA_STALE_SECONDS="${LIVE_DATA_STALE_SECONDS:-120}"
LIVE_DATA_MIN_SPEED="${LIVE_DATA_MIN_SPEED:-1.0}"
OSRM_ALGORITHM="${OSRM_ALGORITHM:-mld}"
OSRM_PORT="${OSRM_PORT:-7070}"

# Preprocessing tools and other binaries pass through unchanged.
case "${1:-}" in
  osrm-extract|osrm-partition|osrm-customize|osrm-contract|osrm-datastore)
    exec "$@"
    ;;
esac

# Default: HTTP server with live traffic enabled.
if [[ $# -eq 0 ]]; then
  echo "Usage: docker run ... <dataset.osrm>" >&2
  echo "   or: docker run ... osrm-extract ..." >&2
  exit 1
fi

DATASET="$1"
shift || true

exec osrm-routed \
  --enable-live-data \
  --port "${OSRM_PORT}" \
  --live-data-udp-port "${LIVE_DATA_UDP_PORT}" \
  --live-data-stale-seconds "${LIVE_DATA_STALE_SECONDS}" \
  --live-data-min-speed "${LIVE_DATA_MIN_SPEED}" \
  --algorithm "${OSRM_ALGORITHM}" \
  "${DATASET}" \
  "$@"
