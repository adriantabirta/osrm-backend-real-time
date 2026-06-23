#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

if [[ -f "${PID_FILE}" ]]; then
  pid=$(cat "${PID_FILE}")
  if kill -0 "${pid}" 2>/dev/null; then
    echo "Stopping osrm-routed (pid ${pid})..."
    kill "${pid}" 2>/dev/null || true
    wait "${pid}" 2>/dev/null || true
  fi
  rm -f "${PID_FILE}"
fi
