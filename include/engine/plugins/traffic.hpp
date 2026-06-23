#ifndef TRAFFIC_PLUGIN_HPP
#define TRAFFIC_PLUGIN_HPP

#include "engine/api/traffic_parameters.hpp"
#include "engine/plugins/plugin_base.hpp"
#include "engine/routing_algorithms.hpp"

namespace osrm::engine::plugins
{

class TrafficPlugin final : public BasePlugin
{
  public:
    explicit TrafficPlugin(bool live_data_enabled) : BasePlugin(boost::none), live_data_enabled_(live_data_enabled)
    {
    }

    Status HandleRequest(const RoutingAlgorithmsInterface &algorithms,
                         const api::TrafficParameters &params,
                         osrm::engine::api::ResultT &result) const;

  private:
    bool live_data_enabled_;
};

} // namespace osrm::engine::plugins

#endif
