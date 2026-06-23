// include/engine/live_data_store.hpp
#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace osrm::engine {

struct TrafficSample {
    uint64_t user_id = 0;
    double latitude = 0.0;
    double longitude = 0.0;
    float speed_kmh = 0.0f;
    float bearing_deg = 0.0f;
    int64_t timestamp_ms = 0;
};

struct TrafficEdgeData {
    double speed_kmh = 0.0;
    double weight_factor = 1.0;
    double bearing_deg = 0.0;
    double latitude = 0.0;
    double longitude = 0.0;
    int64_t timestamp_ms = 0;
    uint32_t sample_count = 0;
    std::string source = "static";
};

class LiveDataStore {
  public:
    static LiveDataStore &instance()
    {
        static LiveDataStore inst;
        return inst;
    }

    void addSample(TrafficSample sample)
    {
        std::unique_lock lock(mutex_);
        pending_samples_.push_back(std::move(sample));
    }

    std::vector<TrafficSample> takePendingSamples()
    {
        std::unique_lock lock(mutex_);
        return std::exchange(pending_samples_, {});
    }

    void update(uint64_t geometry_id, TrafficEdgeData data)
    {
        std::unique_lock lock(mutex_);
        segments_[geometry_id] = std::move(data);
    }

    std::optional<TrafficEdgeData> get(uint64_t geometry_id) const
    {
        std::shared_lock lock(mutex_);
        auto it = segments_.find(geometry_id);
        if (it != segments_.end())
            return it->second;
        return std::nullopt;
    }

    std::vector<std::pair<uint64_t, TrafficEdgeData>> getAllSegments() const
    {
        std::shared_lock lock(mutex_);
        std::vector<std::pair<uint64_t, TrafficEdgeData>> result;
        result.reserve(segments_.size());
        for (const auto &[geometry_id, data] : segments_)
            result.emplace_back(geometry_id, data);
        return result;
    }

    void removeStaleSegments(int stale_seconds)
    {
        const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch())
                                .count();
        const auto max_age_ms = static_cast<int64_t>(stale_seconds) * 1000;

        std::unique_lock lock(mutex_);
        for (auto it = segments_.begin(); it != segments_.end();)
        {
            if (now_ms - it->second.timestamp_ms > max_age_ms)
                it = segments_.erase(it);
            else
                ++it;
        }
    }

    size_t size() const
    {
        std::shared_lock lock(mutex_);
        return segments_.size();
    }

    size_t pendingSampleCount() const
    {
        std::shared_lock lock(mutex_);
        return pending_samples_.size();
    }

    void clear()
    {
        std::unique_lock lock(mutex_);
        segments_.clear();
        pending_samples_.clear();
    }

    void setStaleSeconds(int stale_seconds) { stale_seconds_ = stale_seconds; }

    int staleSeconds() const { return stale_seconds_; }

  private:
    LiveDataStore() = default;

    mutable std::shared_mutex mutex_;
    std::unordered_map<uint64_t, TrafficEdgeData> segments_;
    std::vector<TrafficSample> pending_samples_;
    int stale_seconds_ = 120;
};

} // namespace osrm::engine
