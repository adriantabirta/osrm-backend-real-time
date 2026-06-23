#include "engine/plugins/traffic.hpp"

#include "engine/api/traffic_api.hpp"

namespace osrm::engine::plugins
{

Status TrafficPlugin::HandleRequest(const RoutingAlgorithmsInterface &algorithms,
                                    const api::TrafficParameters &params,
                                    osrm::engine::api::ResultT &result) const
{
    result = util::json::Object();

    if (!live_data_enabled_)
    {
        return Error("LiveDataDisabled",
                     "Live traffic data is not enabled. Start OSRM with --enable-live-data.",
                     result);
    }

    if (!params.IsValid())
        return Error("InvalidOptions", "Both south/west and north/east must be provided for bbox filtering", result);

    const auto &facade = algorithms.GetFacade();
    api::TrafficAPI traffic_api(facade, params);
    traffic_api.MakeResponse(result);
    return Status::Ok;
}

} // namespace osrm::engine::plugins
