#!/bin/bash

# OSRM Live Traffic Data End-to-End Test Script
# 
# This script demonstrates how to:
# 1. Compile the live traffic publisher
# 2. Start OSRM with live data enabled
# 3. Send traffic packets
# 4. Query routes and observe speed changes

set -e

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
REALTIME_DIR="$PROJECT_DIR/osrm-realtime"

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║  OSRM Live Traffic Data - End-to-End Test                     ║"
echo "╚═══════════════════════════════════════════════════════════════╝"

# ─── Step 1: Compile the UDP traffic publisher ────────────────────────
echo ""
echo "📦 Step 1: Compiling live_traffic_publisher..."
if [ ! -f "$REALTIME_DIR/live_traffic_publisher.cpp" ]; then
    echo "❌ Error: live_traffic_publisher.cpp not found"
    exit 1
fi

g++ -std=c++17 -O2 \
    "$REALTIME_DIR/live_traffic_publisher.cpp" \
    -o "$REALTIME_DIR/live_traffic_publisher" \
    2>/dev/null || true

if [ ! -f "$REALTIME_DIR/live_traffic_publisher" ]; then
    echo "⚠️  Could not compile publisher (gcc required). Skipping compilation."
    echo "   Use: g++ -std=c++17 osrm-realtime/live_traffic_publisher.cpp -o live_traffic_publisher"
else
    echo "✓ Publisher compiled successfully"
fi

# ─── Step 2: Check OSRM binary ─────────────────────────────────────────
echo ""
echo "🔍 Step 2: Checking OSRM build..."
if [ ! -f "$BUILD_DIR/osrm-routed" ]; then
    echo "❌ Error: osrm-routed binary not found in $BUILD_DIR"
    echo "   Please build OSRM first: cd build && cmake .. && make"
    exit 1
fi
echo "✓ OSRM binary found: $BUILD_DIR/osrm-routed"

# ─── Step 3: Check for OSRM data files ─────────────────────────────────
echo ""
echo "📂 Step 3: Checking for OSRM data files..."
OSRM_DATA=$(ls "$PROJECT_DIR"/*.osrm 2>/dev/null | head -1)
if [ -z "$OSRM_DATA" ]; then
    echo "⚠️  No .osrm data files found in $PROJECT_DIR"
    echo "   To prepare data:"
    echo "   1. Download a .osm.pbf file (e.g., berlin-latest.osm.pbf)"
    echo "   2. Run: osrm-extract berlin-latest.osm.pbf"
    echo "   3. Run: osrm-contract berlin-latest.osrm"
    echo ""
    echo "For testing, you can download a small region:"
    echo "  wget https://download.openstreetmap.fr/extracts/europe/germany/berlin-latest.osm.pbf"
    exit 0
else
    echo "✓ Data file found: $(basename "$OSRM_DATA")"
fi

# ─── Step 4: Instructions for manual testing ──────────────────────────
echo ""
echo "╔═════════════════════════════════════════════════════════════════╗"
echo "║  Manual Testing Instructions                                    ║"
echo "╚═════════════════════════════════════════════════════════════════╝"

echo ""
echo "1️⃣  Terminal 1 - Start OSRM with live data enabled:"
echo "   $ cd $BUILD_DIR"
echo "   $ ./osrm-routed $(basename "$OSRM_DATA") --enable-live-data --live-data-udp-port 9000"
echo ""

echo "2️⃣  Terminal 2 - Send live traffic data (optional):"
echo "   $ cd $REALTIME_DIR"
if [ -f "$REALTIME_DIR/live_traffic_publisher" ]; then
    echo "   $ ./live_traffic_publisher localhost 9000 12345 52.5 13.4 20 0  # Slow traffic"
    echo "   $ sleep 2"
    echo "   $ ./live_traffic_publisher localhost 9000 12345 52.5 13.4 50 0  # Fast traffic"
else
    echo "   # Publisher not available - compile it first"
fi
echo ""

echo "3️⃣  Terminal 3 - Query routes:"
echo "   # Route will use live speed data from step 2"
echo "   $ curl 'http://localhost:5000/route/v1/driving/52.5,13.4;52.51,13.41'"
echo ""

echo "4️⃣  Verify the results:"
echo "   • Check response JSON for 'duration' and 'distance'"
echo "   • Send different speeds and observe route changes"
echo "   • Fast roads will be preferred over slow roads"
echo ""

echo "╔═════════════════════════════════════════════════════════════════╗"
echo "║  Configuration Options                                          ║"
echo "╚═════════════════════════════════════════════════════════════════╝"

echo ""
echo "Available OSRM command-line options for live data:"
echo "  --enable-live-data              Enable real-time traffic updates"
echo "  --live-data-udp-port PORT       UDP port (default: 9000)"
echo "  --live-data-stale-seconds SECS  Data retention time (default: 120)"
echo "  --live-data-min-speed SPEED     Minimum speed validation (default: 1.0)"
echo ""

echo "Example with custom settings:"
echo "  $ ./osrm-routed data.osrm \\"
echo "      --enable-live-data \\"
echo "      --live-data-udp-port 8888 \\"
echo "      --live-data-stale-seconds 60"
echo ""

echo "╔═════════════════════════════════════════════════════════════════╗"
echo "║  Troubleshooting                                                ║"
echo "╚═════════════════════════════════════════════════════════════════╝"

echo ""
echo "If routes don't change with traffic updates:"
echo "  1. Check UDP packets are received:"
echo "     $ tcpdump -i lo -n 'udp port 9000' (Linux)"
echo "  2. Verify edge IDs are correct for your data"
echo "  3. Check data isn't stale (older than 120 seconds)"
echo "  4. Look at OSRM logs for validation errors"
echo ""

echo "If compilation fails:"
echo "  $ cd build && cmake .. && make -j4"
echo ""

echo "For more information:"
echo "  See $PROJECT_DIR/LIVE_DATA_GUIDE.md"
echo ""
