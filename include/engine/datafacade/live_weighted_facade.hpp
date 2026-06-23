// include/engine/datafacade/live_weighted_facade.hpp
#pragma once

#include "engine/datafacade/contiguous_internalmem_datafacade.hpp"
#include "engine/live_data_store.hpp"
#include "engine/traffic_aggregator.hpp"

#include <boost/range.hpp>
#include <memory>
#include <shared_mutex>
#include <unordered_map>

namespace osrm::engine::datafacade {

template <typename T>
auto make_iterator_range(const std::vector<T> &vec)
{
    return boost::iterator_range<typename std::vector<T>::const_iterator>(vec.begin(), vec.end());
}

template <typename AlgorithmT>
class LiveWeightedDataFacade : public ContiguousInternalMemoryDataFacade<AlgorithmT>
{
    using Base = ContiguousInternalMemoryDataFacade<AlgorithmT>;
    using EdgeWeight = extractor::SegmentDataView::SegmentWeightVector::value_type;
    using PackedGeometryID = std::uint32_t;
    using WeightVector = std::vector<EdgeWeight>;

    mutable std::unordered_map<PackedGeometryID, std::shared_ptr<WeightVector>> forward_cache;
    mutable std::unordered_map<PackedGeometryID, std::shared_ptr<WeightVector>> reverse_cache;
    mutable std::shared_mutex cache_mutex;

  public:
    using Base::Base;

    typename Base::WeightForwardRange
    GetUncompressedForwardWeights(const PackedGeometryID id) const override
    {
        AggregateIfNeeded();

        auto base_weights = Base::GetUncompressedForwardWeights(id);
        auto live = LiveDataStore::instance().get(static_cast<uint64_t>(id));

        if (live && live->source == "live")
        {
            std::shared_lock read_lock(cache_mutex);
            auto it = forward_cache.find(id);
            if (it != forward_cache.end())
                return make_iterator_range(*it->second);
            read_lock.unlock();

            auto modified = std::make_shared<WeightVector>();
            modified->reserve(base_weights.size());
            for (auto w : base_weights)
                modified->push_back(w * live->weight_factor);

            std::unique_lock write_lock(cache_mutex);
            forward_cache[id] = modified;
            return make_iterator_range(*modified);
        }

        return base_weights;
    }

    typename Base::WeightReverseRange
    GetUncompressedReverseWeights(const PackedGeometryID id) const override
    {
        AggregateIfNeeded();

        auto base_weights = Base::GetUncompressedReverseWeights(id);
        auto live = LiveDataStore::instance().get(static_cast<uint64_t>(id));

        if (live && live->source == "live")
        {
            std::shared_lock read_lock(cache_mutex);
            auto it = reverse_cache.find(id);
            if (it != reverse_cache.end())
                return boost::reversed_range<const typename Base::WeightForwardRange>(
                    make_iterator_range(*it->second));
            read_lock.unlock();

            auto modified = std::make_shared<WeightVector>();
            modified->reserve(base_weights.size());
            for (auto w : base_weights)
                modified->push_back(w * live->weight_factor);

            std::unique_lock write_lock(cache_mutex);
            reverse_cache[id] = modified;
            return boost::reversed_range<const typename Base::WeightForwardRange>(
                make_iterator_range(*modified));
        }

        return base_weights;
    }

  private:
    void AggregateIfNeeded() const
    {
        if (LiveDataStore::instance().pendingSampleCount() == 0)
            return;

        AggregatePendingTrafficSamples(static_cast<const Base &>(*this),
                                       LiveDataStore::instance().staleSeconds());

        std::unique_lock write_lock(cache_mutex);
        forward_cache.clear();
        reverse_cache.clear();
    }
};

} // namespace osrm::engine::datafacade
