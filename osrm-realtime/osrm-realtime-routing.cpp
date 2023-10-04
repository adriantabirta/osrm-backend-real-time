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

    EngineConfig config;

    config.storage_config = {"data/moldova-latest.osrm"};
    config.use_shared_memory = false;
    config.algorithm = EngineConfig::Algorithm::MLD;

    const OSRM osrm{config};

    RouteParameters params;
    params.steps = true;
    params.geometries = RouteParameters::GeometriesType::GeoJSON;
    params.overview = RouteParameters::OverviewType::Full;
    params.annotations = true;

    if (argc < 5)
    {
        std::cerr << red
                  << "Call with at least 4 initial parameters (2 coordinates). ex: $ "
                     "osrm-realtime-routing lat1 long1 lat2 long2"
                  << reset << std::endl;
        return EXIT_FAILURE;
    }

    for (int i = 1; i < argc; i += 2)
    {
        // add  coordinates
        float index = i / 2;
        params.coordinates.push_back({util::FloatLongitude{std::stof(argv[i])},
                                      util::FloatLatitude{std::stof(argv[i + 1])}});
        // std::cout << index << " lat: " << argv[i] << " long: " << argv[i + 1] << std::endl;
    }

    engine::api::ResultT result = json::Object();
    const auto status = osrm.Route(params, result);

    auto &json_result = result.get<json::Object>();

    std::string ratesString;
    std::string speedsString;

    if (status == Status::Ok)
    {
        // std::cout << "Status::OK -> before return result" << std::endl;

        auto &routes = json_result.values["routes"].get<json::Array>();
        auto &route = routes.values.at(0).get<json::Object>();

        const auto totalDistance = route.values["distance"].get<json::Number>().value;
        const auto totalDuration = route.values["duration"].get<json::Number>().value;

        auto &legs = route.values["legs"].get<json::Array>();

        for (int i = 0; i < legs.values.size(); i++)
        {
            auto &leg = legs.values.at(i).get<json::Object>();
            auto &steps = leg.values["steps"].get<json::Array>();
            // std::cout << "nr of steps: " << steps.values.size() << std::endl;

            auto &annotation = leg.values["annotation"].get<json::Object>();
            auto &distances = annotation.values["distance"].get<json::Array>(); // in m
            auto &speeds = annotation.values["speed"].get<json::Array>();       // in km/h

            for (int d = 0; d < distances.values.size(); d++)
            {
                const auto rate = distances.values.at(d).get<json::Number>().value;
                ratesString.append(std::to_string(rate / totalDistance));
                ratesString.append(" ");
            }

            for (int s = 0; s < speeds.values.size(); s++)
            {
                const auto speed = speeds.values.at(s).get<json::Number>().value;
                speedsString.append(std::to_string(speed));
                speedsString.append(" ");
            }

            for (int j = 0; j < steps.values.size(); j++)
            {
                auto &step = steps.values.at(j).get<json::Object>();
                auto &geometry = step.values["geometry"].get<json::Object>();
                auto &coordinates = geometry.values["coordinates"].get<json::Array>();
                // std::cout << "coordinates count: " << coordinates.values.size() << std::endl;
                for (int k = 0; k < coordinates.values.size(); k++)
                {
                    auto &coordinate = coordinates.values.at(k).get<json::Array>();
                    std::cout << yellow << coordinate.values.at(0).get<json::Number>().value << " "
                              << coordinate.values.at(1).get<json::Number>().value << " " << reset;
                }
            }
        }

        std::cout << "" << std::endl;
        std::cout << green << ratesString << reset << std::endl;
        std::cout << cyan << speedsString << reset << std::endl;
        std::cout << red << totalDistance << reset << std::endl;
        std::cout << magenta << totalDuration << reset << std::endl;

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