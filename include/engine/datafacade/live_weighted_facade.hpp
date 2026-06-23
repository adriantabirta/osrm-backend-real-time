// include/engine/datafacade/live_weighted_facade.hpp
#pragma once
#include "engine/datafacade/contiguous_internalmem_datafacade.hpp"
#include "engine/live_data_store.hpp"

namespace osrm::engine::datafacade {

template <typename AlgorithmT>
class LiveWeightedDataFacade 
    : public ContiguousInternalMemoryDataFacade<AlgorithmT>
{
    using Base = ContiguousInternalMemoryDataFacade<AlgorithmT>;

public:
    // Preia același constructor ca baza
    using Base::Base;

    // Suprascrie weights forward per segment
    // Aceasta e metoda apelată de algoritmul de rutare pentru fiecare segment
    WeightForwardRange GetUncompressedForwardWeights(
        const GeometryID id) const override
    {
        // Ia weights-urile statice din bază
        auto base_weights = Base::GetUncompressedForwardWeights(id);

        // Dacă avem date live pentru edge-ul curent, le aplicăm
        // EdgeID-ul îl calculăm din GeometryID
        auto live = LiveDataStore::instance().get(
            static_cast<std::uint64_t>(id.id)
        );

        if (live && live->source != "static") {
            // Returnăm un range modificat
            // (implementare simplă — vezi nota de mai jos)
            modified_weights_cache_[id.id] = applyFactor(base_weights, live->weight_factor);
            return rangeOf(modified_weights_cache_[id.id]);
        }

        return base_weights;
    }

    // La fel pentru reverse (dacă folosești rute bidirecționale)
    WeightReverseRange GetUncompressedReverseWeights(
        const GeometryID id) const override
    {
        auto live = LiveDataStore::instance().get(
            static_cast<std::uint64_t>(id.id)
        );
        
        auto base_weights = Base::GetUncompressedReverseWeights(id);
        
        if (live && live->source != "static") {
            modified_weights_rev_cache_[id.id] = applyFactor(base_weights, live->weight_factor);
            return reverseRangeOf(modified_weights_rev_cache_[id.id]);
        }
        
        return base_weights;
    }

private:
    // cache per cerere — se resetează la fiecare request nou
    mutable std::unordered_map<unsigned, std::vector<EdgeWeight>> modified_weights_cache_;
    mutable std::unordered_map<unsigned, std::vector<EdgeWeight>> modified_weights_rev_cache_;

    template <typename Range>
    std::vector<EdgeWeight> applyFactor(const Range& range, double factor) const {
        std::vector<EdgeWeight> result;
        result.reserve(std::distance(range.begin(), range.end()));
        for (auto w : range) {
            result.push_back(static_cast<EdgeWeight>(
                static_cast<double>(w) * factor
            ));
        }
        return result;
    }
};

} // namespace osrm::engine::datafacade
