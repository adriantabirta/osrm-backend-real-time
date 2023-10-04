#include "osrm/match_parameters.hpp"
#include "osrm/nearest_parameters.hpp"
#include "osrm/route_parameters.hpp"
#include "osrm/table_parameters.hpp"
#include "osrm/trip_parameters.hpp"

#include "osrm/coordinate.hpp"
#include "osrm/engine_config.hpp"
#include "osrm/json_container.hpp"

#include "osrm/osrm.hpp"
#include "osrm/status.hpp"

#include <exception>
#include <iostream>
#include <string>
#include <utility>

#include <cstdlib>
#include <typeinfo>

int main(int argc, const char *argv[])
{

    using namespace osrm;

    const std::string red("\033[0;31m");
    const std::string green("\033[1;32m");
    const std::string yellow("\033[1;33m");
    const std::string cyan("\033[0;36m");
    const std::string magenta("\033[0;35m");
    const std::string reset("\033[0m");

    if (argc < 5)
    {
        std::cerr << red
                  << "Call with at least 4 initial parameters (2 coordinates). ex: $ "
                     "osrm-realtime-routing lat1 long1 lat2 long2"
                  << reset << std::endl;
        return EXIT_FAILURE;
    }

    MatchParameters params;

    for (int i = 1; i < argc; i += 2)
    {
        params.coordinates.push_back({util::FloatLongitude{std::stof(argv[i])},
                                      util::FloatLatitude{std::stof(argv[i + 1])}});
        // float index = i / 2;
        // std::cout << index << " lat: " << argv[i] << " long: " << argv[i + 1] << std::endl;
    }

    EngineConfig config;

    config.storage_config = {"data/moldova-latest.osrm"};
    config.use_shared_memory = false;
    config.algorithm = EngineConfig::Algorithm::MLD;

    params.steps = true;
    params.geometries = RouteParameters::GeometriesType::GeoJSON;
    params.overview = RouteParameters::OverviewType::Full;
    params.annotations = true;
    params.annotations = true;

    engine::api::ResultT result = json::Object();

    const OSRM osrm{config};
    const auto status = osrm.Match(params, result);

    auto &json_result = result.get<json::Object>();

    std::string nodeIds;

    if (status == Status::Ok)
    {

        auto &matchings = json_result.values["matchings"].get<json::Array>();
        auto &firstMatch = routes.values.at(0).get<json::Object>();
        auto &geometry = firstMatch.values["geometry"].get<json::Object>();


        auto &coordinates = geometry.values["coordinates"].get<json::Array>();

        for (int i = 0; i < coordinates.values.size(); i++)
        {
            auto &coordinate = coordinates.values.at(i).get<json::Array>();
            std::cout << yellow << coordinate.values.at(0).get<json::Number>().value << " "
                      << coordinate.values.at(1).get<json::Number>().value << " " << reset;
        }

        auto &legs = firstMatch.values["legs"].get<json::Array>();

        for (int i = 0; i < legs.values.size(); i++)
        {
            auto &leg = legs.values.at(i).get<json::Object>();

            auto &annotation = leg.values["annotation"].get<json::Object>();
            auto &nodes = annotation.values["nodes"].get<json::Array>();

            for (int j = 0; j < nodes.values.size(); j++)
            {
                const auto nodeId = nodes.values.at(j).get<json::Number>().value;
                nodeIds.append(std::to_string(nodeId));
                nodeIds.append(" ");
            }
        }

        std::cout << "" << std::endl;
        std::cout << green << nodeIds << reset << std::endl;

        return EXIT_SUCCESS;
    }
    else if (status == Status::Error)
    {
        const auto code = json_result.values["code"].get<json::String>().value;
        const auto message = json_result.values["message"].get<json::String>().value;

        std::cout << "Code: " << code << "\n";
        std::cout << "Message: " << code << "\n";
        return EXIT_FAILURE;
    }
}