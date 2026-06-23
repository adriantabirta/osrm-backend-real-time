// include/engine/traffic_aggregator.hpp
#pragma once

#include "engine/approach.hpp"
#include "engine/datafacade/datafacade_base.hpp"
#include "engine/live_data_store.hpp"

#include "util/typedefs.hpp"

#include "util/coordinate.hpp"

#include <boost/optional.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace osrm::engine {

inline double ComputeTrafficWeightFactor(double speed_kmh)
{
    constexpr double REFERENCE_SPEED = 50.0;
    if (speed_kmh <= 0.5)
        return 10.0;
    const double factor = REFERENCE_SPEED / speed_kmh;
    return factor > 10.0 ? 10.0 : factor;
}

inline std::string SpeedToColor(double speed_kmh)
{
    if (speed_kmh >= 50.0)
        return "#22c55e";
    if (speed_kmh >= 30.0)
        return "#eab308";
    if (speed_kmh >= 15.0)
        return "#f97316";
    return "#ef4444";
}

inline void ApplyLiveTrafficToSegmentWeights(const PackedGeometryID geometry_id,
                                             std::vector<SegmentWeight> &weights)
{
    auto live = LiveDataStore::instance().get(static_cast<std::uint64_t>(geometry_id));
    if (!live || live->source != "live")
        return;

    for (auto &weight : weights)
    {
        weight = to_alias<SegmentWeight>(
            std::round(from_alias<double>(weight) * live->weight_factor));
    }
}

inline void ApplyLiveTrafficToSegmentWeight(const PackedGeometryID geometry_id, SegmentWeight &weight)
{
    auto live = LiveDataStore::instance().get(static_cast<std::uint64_t>(geometry_id));
    if (!live || live->source != "live")
        return;

    weight = to_alias<SegmentWeight>(std::round(from_alias<double>(weight) * live->weight_factor));
}

inline void AggregatePendingTrafficSamples(const datafacade::BaseDataFacade &facade,
                                           int stale_seconds)
{
    auto samples = LiveDataStore::instance().takePendingSamples();
    if (samples.empty())
        return;

    const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();

    // geometry_id -> user_id -> latest sample on that road
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, TrafficSample>> grouped;

    for (const auto &sample : samples)
    {
        if (sample.timestamp_ms + static_cast<int64_t>(stale_seconds) * 1000 < now_ms)
            continue;

        const util::Coordinate coordinate{util::FloatLongitude{sample.longitude},
                                          util::FloatLatitude{sample.latitude}};

        auto phantoms =
            facade.NearestPhantomNodes(coordinate, 1, boost::none, boost::none, Approach::UNRESTRICTED);
        if (phantoms.empty())
            continue;

        const auto &phantom = phantoms.front().phantom_node;
        if (!phantom.forward_segment_id.enabled)
            continue;

        const auto geometry_id =
            static_cast<uint64_t>(facade.GetGeometryIndex(phantom.forward_segment_id.id).id);

        auto &user_samples = grouped[geometry_id];
        auto existing = user_samples.find(sample.user_id);
        if (existing == user_samples.end() || sample.timestamp_ms >= existing->second.timestamp_ms)
            user_samples[sample.user_id] = sample;
    }

    for (const auto &[geometry_id, user_samples] : grouped)
    {
        double speed_sum = 0.0;
        double lat_sum = 0.0;
        double lon_sum = 0.0;
        double bearing_sum = 0.0;
        int64_t latest_ts = 0;

        for (const auto &[user_id, sample] : user_samples)
        {
            (void)user_id;
            speed_sum += sample.speed_kmh;
            lat_sum += sample.latitude;
            lon_sum += sample.longitude;
            bearing_sum += sample.bearing_deg;
            latest_ts = std::max(latest_ts, sample.timestamp_ms);
        }

        const auto count = user_samples.size();
        TrafficEdgeData data;
        data.speed_kmh = speed_sum / static_cast<double>(count);
        data.weight_factor = ComputeTrafficWeightFactor(data.speed_kmh);
        data.bearing_deg = bearing_sum / static_cast<double>(count);
        data.latitude = lat_sum / static_cast<double>(count);
        data.longitude = lon_sum / static_cast<double>(count);
        data.timestamp_ms = latest_ts;
        data.sample_count = static_cast<uint32_t>(count);
        data.source = "live";

        LiveDataStore::instance().update(geometry_id, std::move(data));
    }

    LiveDataStore::instance().removeStaleSegments(stale_seconds);
}

} // namespace osrm::engine
