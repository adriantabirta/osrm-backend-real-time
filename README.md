# OSRM Backend — Live Traffic

Fork OSRM cu trafic live (Waze-like): GPS prin UDP → agregare viteze → rutare + `/traffic/v1` pentru Mapbox.

**Platformă:** Linux (Ubuntu 22.04+, Debian 12+). Docker recomandat pentru producție.

**Porturi implicite:** HTTP `7070`, UDP trafic `9900` (evită conflicte cu AirPlay/5000, etc.)

## Quick start

```bash
./scripts/install-deps-ubuntu.sh
./scripts/build.sh
./scripts/prepare-moldova-data.sh
./scripts/run-all-tests.sh
```

## Scripturi

| Script | Rol |
|--------|-----|
| `scripts/install-deps-ubuntu.sh` | Dependențe sistem |
| `scripts/build.sh` | Compilează binarele |
| `scripts/prepare-moldova-data.sh` | Descarcă + procesează harta Moldova |
| `scripts/start-osrm-server.sh` | Pornește serverul cu live data |
| `scripts/stop-osrm-server.sh` | Oprește serverul |
| `scripts/test-unit.sh` | Teste C++ (fără server) |
| `scripts/test-stress.sh` | Stress UDP+HTTP (necesită server) |
| `scripts/run-all-tests.sh` | Unit + stress (pornește/oprește server) |

Variabile comune (`scripts/common.sh`): `OSRM_PORT=7070`, `UDP_PORT=9900`, `OSRM_URL`, `BUILD_DIR`, `OSRM_DATA`.

## Server

```bash
./build/osrm-routed data/moldova-latest.osrm \
  --enable-live-data --port 7070 --live-data-udp-port 9900 --algorithm mld
```

Sau: `./scripts/start-osrm-server.sh`

## Docker

```bash
./scripts/prepare-moldova-data.sh
docker compose up --build
# HTTP: localhost:7070  UDP: localhost:9900
```

## UDP (Protobuf TrafficBatch)

Schema: [`proto/traffic.proto`](proto/traffic.proto)

```bash
./build/live_traffic_publisher localhost 9900 42 47.01 28.86 90 45
```

## API

```bash
curl "http://localhost:7070/route/v1/driving/28.86,47.01;28.88,47.02"
curl "http://localhost:7070/traffic/v1/driving/"
```

## Licență

BSD-2-Clause — bazat pe [Project OSRM](https://github.com/Project-OSRM/osrm-backend).

Detalii: [LIVE_DATA_GUIDE.md](LIVE_DATA_GUIDE.md).
