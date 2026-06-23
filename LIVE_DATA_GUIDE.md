# OSRM Real-Time Live Traffic Data

This document explains how to use the real-time live traffic data feature in OSRM to influence routing decisions based on current traffic speeds.

## Overview

The system consists of three main components:

1. **UDP Traffic Data Receiver** - OSRM receives traffic updates via UDP packets
2. **Live Data Store** - In-memory cache of current traffic speeds per road segment
3. **Dynamic Routing** - Routes avoid congested segments by increasing their cost

## How It Works

### Traffic Data Flow

```
GPS Trackers / Traffic Sensors
         ↓ (UDP packets)
   OSRM UDP Port 9000
         ↓
  TrafficUpdater thread
         ↓
  LiveDataStore (in-memory cache)
         ↓
  LiveWeightedDataFacade (during routing)
         ↓
  Route requests get current speeds
```

### Speed to Weight Conversion

When live traffic data is available for a road segment:

- **Reference Speed**: 50 km/h (normal/free-flow speed)
- **Weight Factor** = Reference Speed / Actual Speed
- **Applied Weight** = Base Weight × Weight Factor

Examples:
- Current speed 50 km/h → factor 1.0 (normal routing)
- Current speed 25 km/h → factor 2.0 (twice as expensive/slow)
- Current speed 5 km/h  → factor 10.0 (heavily congested, capped at 10x)
- Current speed 1 km/h  → factor 10.0 (gridlock, max penalty)

## Configuration

### Enable Live Data in OSRM Config

```cpp
osrm::engine::EngineConfig config;
config.storage_config = {"/path/to/data/files"};
config.use_live_data = true;                    // Enable live data feature
config.live_data_udp_port = 9000;               // Listen port (default)
config.live_data_stale_seconds = 120;           // Remove data older than 2 min
config.live_data_min_speed_kmh = 1.0;           // Minimum valid speed
```

### Building with Live Data Support

The project already has live data support compiled in. If rebuilding:

```bash
cd build
cmake ..
make -j4
```

## UDP Packet Format

Traffic data is sent as UDP packets with the following structure (40 bytes):

```c
struct TrafficPacket {
    uint64_t edge_id;       // OSM way ID or OSRM edge ID (8 bytes)
    double   latitude;      // WGS84 latitude (8 bytes)
    double   longitude;     // WGS84 longitude (8 bytes)  
    float    speed_kmh;     // Measured speed in km/h (4 bytes)
    float    bearing_deg;   // Direction 0-360, 0=North (4 bytes)
    int64_t  timestamp_ms;  // Unix timestamp in milliseconds (8 bytes)
};
// Total: 40 bytes
```

### UDP Packet Validation

Packets are validated before use:
- ✓ Speed must be 1-300 km/h
- ✓ Latitude must be -90 to +90
- ✓ Longitude must be -180 to +180
- ✓ Data not older than `live_data_stale_seconds`

## Sending Traffic Data

### Using the Example Publisher

A simple C++ UDP client is provided:

```bash
# Compile
g++ -std=c++17 -o live_traffic_publisher osrm-realtime/live_traffic_publisher.cpp

# Send a single traffic packet
# ./live_traffic_publisher <server_ip> <port> <edge_id> <lat> <lon> <speed_kmh> [bearing]
./live_traffic_publisher localhost 9000 12345 52.5 13.4 25.5 90

# Examples:
./live_traffic_publisher localhost 9000 100 52.52 13.40 15  0   # Slow traffic
./live_traffic_publisher localhost 9000 101 52.51 13.41 50  45  # Fast traffic
./live_traffic_publisher localhost 9000 102 52.50 13.42 5   180 # Gridlock
```

### From Other Languages

#### Python
```python
import socket
import struct
import time

def send_traffic_data(host, port, edge_id, lat, lon, speed_kmh, bearing=0):
    packet = struct.pack(
        '<QddffQ',  # unsigned long long, double, double, float, float, long long
        edge_id,
        lat,
        lon,
        speed_kmh,
        bearing,
        int(time.time() * 1000)  # current time in milliseconds
    )
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(packet, (host, port))
    sock.close()

# Send traffic data
send_traffic_data('localhost', 9000, 12345, 52.5, 13.4, 25.5, 90)
```

#### Node.js
```javascript
const dgram = require('dgram');
const client = dgram.createSocket('udp4');

function sendTrafficData(host, port, edgeId, lat, lon, speedKmh, bearing = 0) {
    const now = BigInt(Date.now());
    const buffer = Buffer.alloc(40);
    
    buffer.writeBigUInt64LE(BigInt(edgeId), 0);  // 8 bytes
    buffer.writeDoubleLE(lat, 8);                // 8 bytes
    buffer.writeDoubleLE(lon, 16);               // 8 bytes
    buffer.writeFloatLE(speedKmh, 24);           // 4 bytes
    buffer.writeFloatLE(bearing, 28);            // 4 bytes
    buffer.writeBigInt64LE(now, 32);             // 8 bytes
    
    client.send(buffer, port, host, (err) => {
        if (err) console.error('Send error:', err);
        else console.log('Traffic packet sent');
    });
}

sendTrafficData('localhost', 9000, 12345, 52.5, 13.4, 25.5, 90);
```

## GPS to Edge ID Mapping

To use live traffic data effectively, you need to map GPS points to road segment IDs. Options:

### 1. Use OSRM's Map-Matching Service

OSRM can snap GPS traces to the road network:

```bash
# Map-match GPS trace to find matching edges
curl -X POST "http://localhost:5000/match/v1/driving/13.4,52.5;13.401,52.501?steps=true&annotations=true"
```

The response includes edge indices that can be converted to edge IDs.

### 2. Use Nearest Endpoint

Snap individual points to the nearest road:

```bash
curl "http://localhost:5000/nearest/v1/driving/13.4,52.5?steps=true"
```

### 3. Pre-process with External Tools

- **Vroom**: Vector tile routing engine
- **OSRM Compile Data**: Extract edge IDs from compiled data
- **Custom Scripts**: Build lookup tables mapping coordinates to edge IDs

## Testing

### 1. Start OSRM Server with Live Data Enabled

```bash
# First, prepare OSRM data (if not already done)
./osrm-extract berlin-latest.osm.pbf
./osrm-contract berlin-latest.osrm

# Start server with live data enabled
./osrm-routed berlin-latest.osrm &
```

### 2. Send Test Traffic Data

```bash
# Terminal 1: Start server
./osrm-routed sample.osrm

# Terminal 2: Send traffic updates  
./live_traffic_publisher localhost 9000 12345 52.5 13.4 20 0  # Slow
sleep 2
./live_traffic_publisher localhost 9000 12345 52.5 13.4 50 0  # Fast

# Terminal 3: Query routes
curl "http://localhost:5000/route/v1/driving/52.5,13.4;52.51,13.41"

# The route should change based on traffic updates
```

### 3. Monitor Traffic Data

```bash
# Check live data cache size (requires logging enabled)
# Look for: "LiveDataStore::instance().size()" in logs

# Expected flow:
# 1. UDP packet received
# 2. Data validated and added to cache
# 3. On next routing request, facade reads cache
# 4. Weights adjusted based on current speed
# 5. Router finds path using adjusted weights
```

## Performance Considerations

### Memory Usage
- Each cached entry: ~40 bytes + overhead
- 100,000 segments: ~5 MB
- 1,000,000 segments: ~50 MB

### CPU Impact
- Weight caching reduces recalculation
- Shared mutex for thread-safe access
- Minimal overhead per route request

### Stale Data Handling
- Data older than `live_data_stale_seconds` is automatically removed
- Default: 120 seconds (2 minutes)
- Can be configured per EngineConfig

## Troubleshooting

### Traffic Data Not Being Applied

1. **Check UDP packets are received**
   - Monitor with: `tcpdump -i lo -n "udp port 9000"` (on Linux)
   - Verify source IP and port

2. **Verify Edge IDs**
   - Edge IDs must match the compiled OSRM data
   - Use `/nearest` endpoint to verify edge IDs for your coordinates

3. **Check Data Staleness**
   - Ensure client systems have synchronized time
   - Data older than `live_data_stale_seconds` is ignored

4. **Enable Debug Logging**
   - OSRM logs show traffic packet reception
   - Check for validation errors

### Unexpected Route Changes

1. **Weight Factor Sensitivity**
   - Reference speed of 50 km/h may not match your data
   - Adjust in `live_weighted_facade.hpp` if needed

2. **Capping at 10x**
   - Very slow speeds are capped at 10x weight factor
   - Edit `live_weighted_facade.hpp` to adjust cap

3. **Check Cache**
   - Old data expires after `live_data_stale_seconds`
   - Send periodic updates to keep data fresh

## Future Enhancements

1. **Average Speed Aggregation** - Combine multiple GPS points per edge
2. **Confidence Scores** - Weight data by source reliability  
3. **Time-Based Profiles** - Different speeds for different times of day
4. **Turn Penalties** - Apply live data to turn restrictions too
5. **Persistent Storage** - Save historical traffic patterns
6. **Real-time API** - WebSocket updates instead of polling

## References

- OSRM Documentation: https://project-osrm.org/
- GitHub: https://github.com/Project-OSRM/osrm-backend
- Live Traffic Data Discussion: (See GitHub issues #3852, #4200)
