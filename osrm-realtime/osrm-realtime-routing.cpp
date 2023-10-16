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

    if (argc < 6)
    {
        std::cerr << "Call with at least 5 initial parameters (*.osrm, 2 coordinates). ex: $ "
                     "osrm-realtime-routing *.osrm lat1 long1 lat2 long2"
                  << std::endl;
        return EXIT_FAILURE;
    }

    RouteParameters params;

    // start from 3 element where coordinates start
    for (int i = 2; i < argc; i += 2)
    {
        params.coordinates.push_back({util::FloatLongitude{std::stof(argv[i])},
                                      util::FloatLatitude{std::stof(argv[i + 1])}});
    }

    params.steps = true;
    params.geometries = RouteParameters::GeometriesType::GeoJSON;
    params.overview = RouteParameters::OverviewType::Full;
    params.annotations = true;

    EngineConfig config;

    config.storage_config = {argv[1]};
    config.use_shared_memory = false;
    config.algorithm = EngineConfig::Algorithm::MLD;

    engine::api::ResultT result = json::Object();

    const OSRM osrm{config};
    const auto status = osrm.Route(params, result);

    auto &json_result = result.get<json::Object>();

    std::string nodesString;
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

            auto &annotation = leg.values["annotation"].get<json::Object>();
            auto &distances = annotation.values["distance"].get<json::Array>(); // in m
            auto &speeds = annotation.values["speed"].get<json::Array>();       // in km/h

            auto &nodes = annotation.values["nodes"].get<json::Array>();

            for (int n = 0; n < nodes.values.size() - 1; n++) // iterate until n-1
            {
                const auto node1 = nodes.values.at(n).get<json::Number>().value;
                const auto node2 = nodes.values.at(n+1).get<json::Number>().value;
                const auto speed1 = speeds.values.at(n).get<json::Number>().value;
                nodesString.append(std::to_string(node1) + " " + std::to_string(node2) + " " + std::to_string(speed1) + " ");
            }

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

                for (int k = 0; k < coordinates.values.size(); k++)
                {
                    auto &coordinate = coordinates.values.at(k).get<json::Array>();
                    std::cout << coordinate.values.at(0).get<json::Number>().value << " "
                              << coordinate.values.at(1).get<json::Number>().value << " ";
                }
            }
        }

        std::cout << "" << std::endl;
        std::cout << nodesString << std::endl;
        std::cout << ratesString << std::endl;
        std::cout << speedsString << std::endl;
        std::cout << totalDistance << std::endl;
        std::cout << totalDuration << std::endl;

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