#ifndef ENGINE_API_TRAFFIC_PARAMETERS_HPP
#define ENGINE_API_TRAFFIC_PARAMETERS_HPP

#include "engine/api/base_parameters.hpp"
#include "util/coordinate.hpp"

#include <boost/optional.hpp>

namespace osrm::engine::api
{

struct TrafficParameters : public BaseParameters
{
    enum class GeometriesType { Polyline6, GeoJSON };

    boost::optional<util::Coordinate> south_west;
    boost::optional<util::Coordinate> north_east;
    GeometriesType geometries = GeometriesType::Polyline6;

    bool IsValid() const
    {
        if (!BaseParameters::IsValid())
            return false;
        if (!south_west && !north_east)
            return true;
        return south_west && north_east;
    }
};

} // namespace osrm::engine::api

#endif
