#ifndef ENGINE_API_TRAFFIC_API_HPP
#define ENGINE_API_TRAFFIC_API_HPP

#include "engine/api/traffic_parameters.hpp"
#include "engine/datafacade/datafacade_base.hpp"
#include "engine/live_data_store.hpp"
#include "engine/api/json_factory.hpp"
#include "engine/traffic_aggregator.hpp"

#include "util/coordinate.hpp"
#include "util/json_container.hpp"
#include "util/typedefs.hpp"

#include <vector>

namespace osrm::engine::api
{

class TrafficAPI
{
  public:
    TrafficAPI(const datafacade::BaseDataFacade &facade_, const TrafficParameters &parameters_)
        : facade(facade_), parameters(parameters_)
    {
    }

    void MakeResponse(osrm::engine::api::ResultT &response) const
    {
        auto &json_result = response.get<util::json::Object>();
        MakeResponse(json_result);
    }

    void MakeResponse(util::json::Object &response) const
    {
        AggregatePendingTrafficSamples(facade, LiveDataStore::instance().staleSeconds());

        util::json::Array segments;
        const auto all_segments = LiveDataStore::instance().getAllSegments();

        for (const auto &[geometry_id, data] : all_segments)
        {
            if (data.source != "live")
                continue;

            if (!segmentInBBox(geometry_id, data))
                continue;

            util::json::Object segment;
            segment.values["geometry_id"] = geometry_id;
            segment.values["speed_kmh"] = data.speed_kmh;
            segment.values["avg_speed_kmh"] = data.speed_kmh;
            segment.values["sample_count"] = static_cast<std::uint64_t>(data.sample_count);
            segment.values["weight_factor"] = data.weight_factor;
            segment.values["color"] = SpeedToColor(data.speed_kmh);
            segment.values["timestamp_ms"] = static_cast<std::uint64_t>(data.timestamp_ms);

            auto coordinates = buildCoordinates(geometry_id);
            if (coordinates.empty())
                continue;

            if (parameters.geometries == TrafficParameters::GeometriesType::GeoJSON)
            {
                util::json::Object geometry;
                geometry.values["type"] = util::json::String{"LineString"};

                util::json::Array coords;
                for (const auto &coordinate : coordinates)
                {
                    util::json::Array point;
                    point.values.push_back(
                        util::json::Number{static_cast<double>(util::toFloating(coordinate.lon))});
                    point.values.push_back(
                        util::json::Number{static_cast<double>(util::toFloating(coordinate.lat))});
                    coords.values.push_back(std::move(point));
                }
                geometry.values["coordinates"] = std::move(coords);
                segment.values["geometry"] = std::move(geometry);
            }
            else
            {
                segment.values["polyline"] =
                    json::makePolyline<1000000>(coordinates.begin(), coordinates.end());
            }

            segments.values.push_back(std::move(segment));
        }

        response.values["code"] = "Ok";
        response.values["segments"] = std::move(segments);
    }

  private:
    std::vector<util::Coordinate> buildCoordinates(uint64_t geometry_id) const
    {
        const auto packed_id = static_cast<PackedGeometryID>(geometry_id);
        const auto geometry = facade.GetUncompressedForwardGeometry(packed_id);
        if (geometry.empty())
            return {};

        std::vector<util::Coordinate> coordinates;
        coordinates.reserve(geometry.size());
        for (std::size_t i = 0; i < geometry.size(); ++i)
            coordinates.push_back(facade.GetCoordinateOfNode(geometry(i)));
        return coordinates;
    }

    bool segmentInBBox(uint64_t geometry_id, const TrafficEdgeData &data) const
    {
        if (!parameters.south_west || !parameters.north_east)
            return true;

        const auto coordinates = buildCoordinates(geometry_id);
        if (!coordinates.empty())
        {
            for (const auto &coordinate : coordinates)
            {
                if (coordinate.lat >= parameters.south_west->lat &&
                    coordinate.lat <= parameters.north_east->lat &&
                    coordinate.lon >= parameters.south_west->lon &&
                    coordinate.lon <= parameters.north_east->lon)
                    return true;
            }
            return false;
        }

        const util::Coordinate centroid{util::FloatLongitude{data.longitude},
                                      util::FloatLatitude{data.latitude}};
        return centroid.lat >= parameters.south_west->lat &&
               centroid.lat <= parameters.north_east->lat &&
               centroid.lon >= parameters.south_west->lon && centroid.lon <= parameters.north_east->lon;
    }

    const datafacade::BaseDataFacade &facade;
    const TrafficParameters &parameters;
};

} // namespace osrm::engine::api

#endif
