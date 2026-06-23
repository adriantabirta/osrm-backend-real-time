#include "engine/live_data_store.hpp"
#include "engine/traffic_aggregator.hpp"
#include "engine/traffic_updater.hpp"

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <cstring>

using namespace osrm::engine;

namespace
{
int64_t nowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}
} // namespace

BOOST_AUTO_TEST_SUITE(live_traffic)

BOOST_AUTO_TEST_CASE(traffic_packet_layout_matches_udp_wire_format)
{
    BOOST_CHECK_EQUAL(sizeof(TrafficPacket), 40u);

    TrafficPacket pkt{};
    pkt.user_id = 42;
    pkt.latitude = 52.5;
    pkt.longitude = 13.4;
    pkt.speed_kmh = 33.3f;
    pkt.bearing_deg = 90.0f;
    pkt.timestamp_ms = 1'700'000'000'000;

    unsigned char buffer[40];
    std::memcpy(buffer, &pkt, sizeof(pkt));

    uint64_t user_id = 0;
    std::memcpy(&user_id, buffer, sizeof(user_id));
    BOOST_CHECK_EQUAL(user_id, 42u);

    double latitude = 0.0;
    std::memcpy(&latitude, buffer + 8, sizeof(latitude));
    BOOST_CHECK_CLOSE(latitude, 52.5, 0.001);
}

BOOST_AUTO_TEST_CASE(live_data_store_keeps_pending_samples_and_segments)
{
    LiveDataStore::instance().clear();

    TrafficSample sample;
    sample.user_id = 7;
    sample.latitude = 52.5;
    sample.longitude = 13.4;
    sample.speed_kmh = 20.0f;
    sample.bearing_deg = 180.0f;
    sample.timestamp_ms = nowMs();

    LiveDataStore::instance().addSample(sample);
    BOOST_CHECK_EQUAL(LiveDataStore::instance().pendingSampleCount(), 1u);
    BOOST_CHECK_EQUAL(LiveDataStore::instance().size(), 0u);

    auto pending = LiveDataStore::instance().takePendingSamples();
    BOOST_REQUIRE_EQUAL(pending.size(), 1u);
    BOOST_CHECK_EQUAL(pending.front().user_id, 7u);
    BOOST_CHECK_EQUAL(LiveDataStore::instance().pendingSampleCount(), 0u);

    TrafficEdgeData segment;
    segment.speed_kmh = 30.0;
    segment.weight_factor = ComputeTrafficWeightFactor(30.0);
    segment.sample_count = 2;
    segment.timestamp_ms = nowMs();
    segment.source = "live";

    LiveDataStore::instance().update(123, segment);
    BOOST_CHECK_EQUAL(LiveDataStore::instance().size(), 1u);

    const auto stored = LiveDataStore::instance().get(123);
    BOOST_REQUIRE(stored);
    BOOST_CHECK_CLOSE(stored->speed_kmh, 30.0, 0.001);
    BOOST_CHECK_EQUAL(stored->sample_count, 2u);
    BOOST_CHECK_EQUAL(stored->source, "live");

    LiveDataStore::instance().clear();
}

BOOST_AUTO_TEST_CASE(weight_factor_and_color_mapping)
{
    BOOST_CHECK_CLOSE(ComputeTrafficWeightFactor(50.0), 1.0, 0.001);
    BOOST_CHECK_CLOSE(ComputeTrafficWeightFactor(25.0), 2.0, 0.001);
    BOOST_CHECK_CLOSE(ComputeTrafficWeightFactor(5.0), 10.0, 0.001);
    BOOST_CHECK_CLOSE(ComputeTrafficWeightFactor(1.0), 10.0, 0.001);

    BOOST_CHECK_EQUAL(SpeedToColor(55.0), "#22c55e");
    BOOST_CHECK_EQUAL(SpeedToColor(40.0), "#eab308");
    BOOST_CHECK_EQUAL(SpeedToColor(20.0), "#f97316");
    BOOST_CHECK_EQUAL(SpeedToColor(10.0), "#ef4444");
}

BOOST_AUTO_TEST_CASE(stale_segments_are_removed)
{
    LiveDataStore::instance().clear();
    LiveDataStore::instance().setStaleSeconds(60);

    TrafficEdgeData old_segment;
    old_segment.speed_kmh = 10.0;
    old_segment.weight_factor = 5.0;
    old_segment.timestamp_ms = nowMs() - 120'000;
    old_segment.source = "live";

    LiveDataStore::instance().update(1, old_segment);
    BOOST_CHECK_EQUAL(LiveDataStore::instance().size(), 1u);

    LiveDataStore::instance().removeStaleSegments(60);
    BOOST_CHECK_EQUAL(LiveDataStore::instance().size(), 0u);

    LiveDataStore::instance().clear();
}

BOOST_AUTO_TEST_SUITE_END()
