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

    if (argc < 4)
    {
        std::cerr << "Call with at least 4 initial parameters (*.osrm, 1 coordinate). ex: $ "
                     "osrm-realtime-routing *.osrm lat long"
                  << std::endl;
        return EXIT_FAILURE;
    }

    NearestParameters params;
    params.number_of_results = 1;

    // std::cout << " lat: " << argv[2] << " long: " << argv[3] << std::endl;
    params.coordinates.push_back({util::FloatLongitude{std::stof(argv[2])}, util::FloatLatitude{std::stof(argv[3])}});

    EngineConfig config;

    config.storage_config = {argv[1]};
    config.use_shared_memory = false;
    config.algorithm = EngineConfig::Algorithm::MLD;

    engine::api::ResultT result = json::Object();

    const OSRM osrm{config};
    const auto status = osrm.Nearest(params, result);

    auto &json_result = result.get<json::Object>();

    if (status == Status::Ok)
    {
        auto &waypoints = json_result.values["waypoints"].get<json::Array>();
        auto &firstWaypoint = waypoints.values.at(0).get<json::Object>();
        auto &nodes = firstWaypoint.values["nodes"].get<json::Array>();

        for (int i = 0; i < nodes.values.size(); i++)
        {
            std::cout << std::to_string(nodes.values.at(i).get<json::Number>().value) << " ";
        }
        std::cout << "" << std::endl;

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