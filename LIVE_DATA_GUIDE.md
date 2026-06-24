# OSRM Real-Time Live Traffic Data

This document explains how to use the real-time live traffic data feature in OSRM to influence routing decisions based on current traffic speeds.

## Overview

The system consists of three main components:

1. **UDP Traffic Data Receiver** - OSRM receives GPS traffic updates via UDP packets
2. **Live Data Store** - In-memory cache with average speeds per road segment
3. **Dynamic Routing + Traffic API** - Routes avoid congested segments; Mapbox-ready colored polylines

## How It Works

### Traffic Data Flow

```
GPS Trackers / Mobile Apps
         ↓ (UDP: user_id + lat/lon + speed)
   OSRM UDP Port 9900
         ↓
  TrafficUpdater thread
         ↓
  LiveDataStore (raw samples)
         ↓
  Traffic Aggregator (snap GPS → road, avg speed per segment)
         ↓
  Routing engine (segment weights adjusted from live traffic)
         ↓
  /traffic/v1 API (colored polylines for Mapbox)
```

### Key Design

- **Clients send only `user_id` + GPS data** — no `edge_id`
- **OSRM resolves the road segment** via nearest-road snapping
- **Average speed** is computed per road from all active users on that segment
- **Colors** are assigned by speed for easy Mapbox visualization

### Speed to Weight Conversion

When live traffic data is available for a road segment:

- **Reference Speed**: 50 km/h (normal/free-flow speed)
- **Weight Factor** = Reference Speed / Actual Speed
- **Applied Weight** = Base Weight × Weight Factor

Examples:
- Current speed 50 km/h → factor 1.0 (normal routing)
- Current speed 25 km/h → factor 2.0 (twice as expensive/slow)
- Current speed 5 km/h  → factor 10.0 (heavily congested, capped at 10x)

### Speed Colors (Mapbox)

| Speed (km/h) | Color   | Meaning        |
|--------------|---------|----------------|
| ≥ 50         | #22c55e | Free flow      |
| 30–49        | #eab308 | Moderate       |
| 15–29        | #f97316 | Slow           |
| < 15         | #ef4444 | Congested      |

## Configuration

### Enable Live Data in OSRM Config

```cpp
osrm::engine::EngineConfig config;
config.storage_config = {"/path/to/data/files"};
config.use_live_data = true;
config.live_data_udp_port = 9900;
config.live_data_stale_seconds = 120;
config.live_data_min_speed_kmh = 1.0;
```

### Building with Live Data Support

```bash
cd build
cmake ..
make -j4
```

## UDP Payload (Protobuf)

Clients send **`traffic.TrafficBatch`** over UDP (port **9900** by default). Schema: [`proto/traffic.proto`](proto/traffic.proto).

```protobuf
message TrafficPacket {
  uint64 user_id = 1;
  double latitude = 2;
  double longitude = 3;
  float speed_kmh = 4;
  float bearing_deg = 5;
  int64 timestamp_ms = 6;
}

message TrafficBatch {
  repeated TrafficPacket packets = 1;
}
```

Swift clients can generate types from the same `.proto` (`swift_prefix` supported). The server decodes with **protozero** (no `libprotobuf` runtime dependency).

### Validation

Each packet in a batch is validated before use:
- Speed must be 1–300 km/h
- Latitude −90…+90, longitude −180…+180
- `timestamp_ms` optional; if set, must be within `live_data_stale_seconds`

## Sending Traffic Data

### Using the Example Publisher

```bash
# Build (or use ./scripts/build.sh)
cmake --build build --target live_traffic_publisher

# Send traffic from user 42 at given coordinates
# ./build/live_traffic_publisher <server_ip> <port> <user_id> <lat> <lon> <speed_kmh> [bearing]
./build/live_traffic_publisher localhost 9900 42 47.01 28.86 25.5 90

# Multiple users on the same road → OSRM computes average speed
./build/live_traffic_publisher localhost 9900 42 47.02 28.87 15  0   # slow
./build/live_traffic_publisher localhost 9900 43 47.02 28.87 45  0   # fast → avg ~30 km/h
```

### From Python

```python
import socket
import struct
import time

def send_traffic_data(host, port, user_id, lat, lon, speed_kmh, bearing=0):
    packet = struct.pack(
        '<QddffQ',
        user_id,
        lat,
        lon,
        speed_kmh,
        bearing,
        int(time.time() * 1000)
    )
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(packet, (host, port))
    sock.close()

send_traffic_data('localhost', 9900, 42, 52.5, 13.4, 25.5, 90)
```

## Traffic API for Mapbox

Get road segments with speed, color, and polyline:

```bash
# All active traffic segments (polyline6, Mapbox default)
curl "http://localhost:7070/traffic/v1/driving"

# Filter by bounding box
curl "http://localhost:7070/traffic/v1/driving?south=52.4&west=13.3&north=52.6&east=13.5"

# GeoJSON LineString geometry instead of polyline
curl "http://localhost:7070/traffic/v1/driving?geometries=geojson"
```

### Response Example

```json
{
  "code": "Ok",
  "segments": [
    {
      "geometry_id": 12345,
      "speed_kmh": 25.5,
      "avg_speed_kmh": 25.5,
      "sample_count": 3,
      "weight_factor": 1.96,
      "color": "#f97316",
      "timestamp_ms": 1719148800000,
      "polyline": "encoded_polyline6_string..."
    }
  ]
}
```

### Mapbox GL JS Example

```javascript
const response = await fetch('http://localhost:7070/traffic/v1/driving?geometries=geojson');
const data = await response.json();

const features = data.segments.map(seg => ({
  type: 'Feature',
  properties: {
    speed: seg.speed_kmh,
    color: seg.color
  },
  geometry: seg.geometry
}));

map.addSource('live-traffic', {
  type: 'geojson',
  data: { type: 'FeatureCollection', features }
});

map.addLayer({
  id: 'live-traffic-lines',
  type: 'line',
  source: 'live-traffic',
  paint: {
    'line-color': ['get', 'color'],
    'line-width': 5
  }
});
```

## Testing

### Unit tests (no server required)

```bash
./scripts/test-unit.sh
# sau totul dintr-o dată:
./scripts/run-all-tests.sh
```

Includes packet format, store logic, weight/color mapping, and **concurrency** tests (parallel readers/writers + UDP burst load).

### Performance / concurrency stress test

Requires a running `osrm-routed` with `--enable-live-data` and valid `.osrm` data:

```bash
./scripts/start-osrm-server.sh

# Default: 15s, max 100 UDP pkt/s, 8 concurrent route workers
./scripts/test-stress.sh
```

Tune via environment variables:

```bash
DURATION_SEC=60 MAX_UDP_RATE=100 ROUTE_WORKERS=12 ./scripts/test-stress.sh
```

The stress test sends random **TrafficBatch** UDP messages while concurrently calling `/route/v1` and `/traffic/v1`.

### Manual testing

```bash
./osrm-routed map.osrm --enable-live-data --live-data-udp-port 9900
```

### 2. Send Test Traffic Data

```bash
./live_traffic_publisher localhost 9900 42 52.5 13.4 20 0
./live_traffic_publisher localhost 9900 43 52.5 13.4 40 0
```

### 3. Query Traffic Segments

```bash
curl "http://localhost:7070/traffic/v1/driving"
```

### 4. Query Routes (uses live weights)

```bash
curl "http://localhost:7070/route/v1/driving/52.5,13.4;52.51,13.41"
```

## Performance Considerations

### Memory Usage
- Each cached segment: ~80 bytes + overhead
- Raw samples are processed and aggregated quickly

### CPU Impact
- Aggregation runs when new samples arrive (before routing/traffic queries)
- Weight caching reduces recalculation during routing

### Stale Data Handling
- Data older than `live_data_stale_seconds` is automatically removed (default: 120s)

## Troubleshooting

### Traffic Data Not Being Applied

1. Verify `--enable-live-data` is set
2. Check UDP packets: `tcpdump -i lo -n "udp port 9900"`
3. Ensure coordinates are on the road network (use `/nearest` to verify)
4. Check timestamps are synchronized

### Empty /traffic Response

1. Send UDP packets first, then query `/traffic/v1/driving`
2. Coordinates must snap to a valid road segment
3. Wait for aggregation (happens automatically on next request)

### Routes Not Changing

1. Congestion must exist on alternative routes
2. Verify `sample_count` > 0 in traffic API response
3. Check OSRM logs for TrafficUpdater startup message

## References

- OSRM Documentation: https://project-osrm.org/
- Mapbox Polyline6: https://github.com/mapbox/polyline
