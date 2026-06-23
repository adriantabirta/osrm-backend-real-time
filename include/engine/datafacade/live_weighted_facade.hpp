// include/engine/datafacade/live_weighted_facade.hpp
#pragma once
#include "engine/datafacade/contiguous_internalmem_datafacade.hpp"
#include "engine/live_data_store.hpp"

#include <memory>
#include <unordered_map>
#include <shared_mutex>

#include <boost/range.hpp>

namespace osrm::engine::datafacade {

// Helper class to create boost ranges from vectors
template <typename T>
auto make_iterator_range(const std::vector<T>& vec) {
    return boost::iterator_range<typename std::vector<T>::const_iterator>(vec.begin(), vec.end());
}

template <typename AlgorithmT>
class LiveWeightedDataFacade 
    : public ContiguousInternalMemoryDataFacade<AlgorithmT>
{
    using Base = ContiguousInternalMemoryDataFacade<AlgorithmT>;

    // Re-declare types from base for use in this class
    using EdgeWeight = extractor::SegmentDataView::SegmentWeightVector::value_type;
    using PackedGeometryID = std::uint32_t;
    using WeightVector = std::vector<EdgeWeight>;

    // Mutable cache for modified weights
    mutable std::unordered_map<PackedGeometryID, std::shared_ptr<WeightVector>> forward_cache;
    mutable std::unordered_map<PackedGeometryID, std::shared_ptr<WeightVector>> reverse_cache;
    mutable std::shared_mutex cache_mutex;

public:
    // Preia același constructor ca baza
    using Base::Base;

    // Suprascrie weights forward per segment
    // Aceasta e metoda apelată de algoritmul de rutare pentru fiecare segment
    typename Base::WeightForwardRange GetUncompressedForwardWeights(
        const PackedGeometryID id) const override
    {
        // Ia weights-urile statice din bază
        auto base_weights = Base::GetUncompressedForwardWeights(id);

        // Dacă avem date live pentru edge-ul curent, le aplicăm
        auto live = LiveDataStore::instance().get(static_cast<std::uint64_t>(id));

        if (live && live->source == "live") {
            std::shared_lock read_lock(cache_mutex);
            auto it = forward_cache.find(id);
            if (it != forward_cache.end()) {
                return make_iterator_range(*it->second);
            }
            read_lock.unlock();

            // Create modified weights
            auto modified = std::make_shared<WeightVector>();
            modified->reserve(base_weights.size());
            for (auto w : base_weights) {
                // Multiply the weight by the factor
                modified->push_back(w * live->weight_factor);
            }

            // Store in cache
            std::unique_lock write_lock(cache_mutex);
            forward_cache[id] = modified;
            return make_iterator_range(*modified);
        }

        return base_weights;
    }

    // La fel pentru reverse (dacă folosești rute bidirecționale)
    typename Base::WeightReverseRange GetUncompressedReverseWeights(
        const PackedGeometryID id) const override
    {
        auto base_weights = Base::GetUncompressedReverseWeights(id);
        auto live = LiveDataStore::instance().get(static_cast<std::uint64_t>(id));

        if (live && live->source == "live") {
            std::shared_lock read_lock(cache_mutex);
            auto it = reverse_cache.find(id);
            if (it != reverse_cache.end()) {
                return boost::reversed_range<const typename Base::WeightForwardRange>(make_iterator_range(*it->second));
            }
            read_lock.unlock();

            // Create modified weights
            auto modified = std::make_shared<WeightVector>();
            modified->reserve(base_weights.size());
            for (auto w : base_weights) {
                // Multiply the weight by the factor
                modified->push_back(w * live->weight_factor);
            }

            // Store in cache
            std::unique_lock write_lock(cache_mutex);
            reverse_cache[id] = modified;
            return boost::reversed_range<const typename Base::WeightForwardRange>(make_iterator_range(*modified));
        }

        return base_weights;
    }
};

} // namespace osrm::engine::datafacade
