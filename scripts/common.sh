# Shared helpers for OSRM live-traffic scripts. Source from other scripts:
#   source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT}/build}"
DATA_DIR="${ROOT}/data"
DATASET="${OSRM_DATA:-${DATA_DIR}/moldova-latest.osrm}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"
OSRM_PORT="${OSRM_PORT:-7070}"
OSRM_URL="${OSRM_URL:-http://127.0.0.1:${OSRM_PORT}}"
UDP_PORT="${UDP_PORT:-9900}"
MOLDOVA_PROBE="28.8608,47.0105"
DEFAULT_ROUTE_COORDS="28.8608,47.0105;28.8750,47.0200"
PID_FILE="${DATA_DIR}/.osrm-routed.pid"

CMAKE_ARGS=(
  -S "${ROOT}"
  -B "${BUILD_DIR}"
  -DCMAKE_BUILD_TYPE=Release
  -DBUILD_ROUTED=ON
  -DENABLE_NODE_BINDINGS=OFF
)

ensure_cmake() {
  if [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
    cmake "${CMAKE_ARGS[@]}"
  fi
}

ensure_target() {
  local target="$1"
  ensure_cmake
  case "${target}" in
    engine-tests)
      [[ -x "${BUILD_DIR}/unit_tests/engine-tests" ]] || \
        cmake --build "${BUILD_DIR}" --target engine-tests -j"${JOBS}"
      ;;
    *)
      [[ -x "${BUILD_DIR}/${target}" ]] || \
        cmake --build "${BUILD_DIR}" --target "${target}" -j"${JOBS}"
      ;;
  esac
}

osrm_is_running() {
  curl -sf "${OSRM_URL}/nearest/v1/driving/${MOLDOVA_PROBE}?number=1" >/dev/null 2>&1
}
