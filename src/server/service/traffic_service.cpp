#include "server/service/traffic_service.hpp"

#include "engine/api/traffic_parameters.hpp"
#include "util/coordinate.hpp"
#include "util/json_container.hpp"

#include <cstdlib>
#include <string>

namespace osrm::server::service
{

namespace
{

boost::optional<double> parseQueryValue(const std::string &query, const std::string &key)
{
    const auto key_pos = query.find(key + "=");
    if (key_pos == std::string::npos)
        return boost::none;

    const auto value_start = key_pos + key.size() + 1;
    const auto value_end = query.find('&', value_start);
    const auto value = query.substr(value_start,
                                    value_end == std::string::npos ? std::string::npos
                                                                   : value_end - value_start);
    if (value.empty())
        return boost::none;
    return std::stod(value);
}

engine::api::TrafficParameters parseTrafficParameters(const std::string &query)
{
    engine::api::TrafficParameters parameters;

    const auto south = parseQueryValue(query, "south");
    const auto west = parseQueryValue(query, "west");
    const auto north = parseQueryValue(query, "north");
    const auto east = parseQueryValue(query, "east");

    if (south && west && north && east)
    {
        parameters.south_west = util::Coordinate{util::FloatLongitude{*west}, util::FloatLatitude{*south}};
        parameters.north_east = util::Coordinate{util::FloatLongitude{*east}, util::FloatLatitude{*north}};
    }

    const auto geometries_pos = query.find("geometries=");
    if (geometries_pos != std::string::npos)
    {
        const auto value_start = geometries_pos + 11;
        const auto value_end = query.find('&', value_start);
        const auto value = query.substr(value_start,
                                        value_end == std::string::npos ? std::string::npos
                                                                       : value_end - value_start);
        if (value == "geojson")
            parameters.geometries = engine::api::TrafficParameters::GeometriesType::GeoJSON;
    }

    return parameters;
}

} // namespace

engine::Status TrafficService::RunQuery(std::size_t /*prefix_length*/,
                                        std::string &query,
                                        osrm::engine::api::ResultT &result)
{
    result = util::json::Object();
    auto &json_result = result.get<util::json::Object>();

    try
    {
        const auto parameters = parseTrafficParameters(query);
        if (!parameters.IsValid())
        {
            json_result.values["code"] = "InvalidOptions";
            json_result.values["message"] =
                "Invalid bbox. Provide south, west, north and east together.";
            return engine::Status::Error;
        }

        return BaseService::routing_machine.Traffic(parameters, result);
    }
    catch (const std::exception &ex)
    {
        json_result.values["code"] = "InvalidQuery";
        json_result.values["message"] = ex.what();
        return engine::Status::Error;
    }
}

} // namespace osrm::server::service
