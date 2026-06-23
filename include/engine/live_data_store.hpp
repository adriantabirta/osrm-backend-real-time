// include/engine/live_data_store.hpp
#pragma once
#include <shared_mutex>
#include <unordered_map>
#include <optional>
#include <cstdint>
#include <string>
#include <chrono>

namespace osrm::engine {

struct TrafficEdgeData {
    double   speed_kmh     = 0.0;
    double   weight_factor = 1.0;
    double   bearing_deg   = 0.0;   // din pachetul UDP
    double   latitude      = 0.0;   // coordonata ultimei măsurători
    double   longitude     = 0.0;
    int64_t  timestamp_ms  = 0;     // când a fost primit pachetul
    std::string source     = "static"; // "live" | "static"
};

class LiveDataStore {
public:
    static LiveDataStore& instance() {
        static LiveDataStore inst;
        return inst;
    }

    void update(uint64_t edge_id, TrafficEdgeData data) {
        std::unique_lock lock(mutex_);
        edges_[edge_id] = std::move(data);
    }

    std::optional<TrafficEdgeData> get(uint64_t edge_id) const {
        std::shared_lock lock(mutex_);
        auto it = edges_.find(edge_id);
        if (it != edges_.end()) return it->second;
        return std::nullopt;
    }

    // Numărul de edge-uri cu date live în memorie
    size_t size() const {
        std::shared_lock lock(mutex_);
        return edges_.size();
    }

    void clear() {
        std::unique_lock lock(mutex_);
        edges_.clear();
    }

private:
    LiveDataStore() = default;
    mutable std::shared_mutex mutex_;
    std::unordered_map<uint64_t, TrafficEdgeData> edges_;
};

} // namespace osrm::engine
