#include "engine/traffic_protobuf.hpp"

#include <iostream>
#include <stdexcept>

int main()
{
    osrm::engine::traffic_proto::TrafficPacketMsg packet;
    packet.user_id = 99;
    packet.latitude = 44.4;
    packet.longitude = 26.1;
    packet.speed_kmh = 42.0f;
    packet.bearing_deg = 180.0f;
    packet.timestamp_ms = 1'700'000'000'000;

    const auto wire = osrm::engine::traffic_proto::encodeTrafficBatch(packet);

    std::size_t parsed = 0;
    parsed = osrm::engine::traffic_proto::parseTrafficBatch(
        wire.data(), wire.size(), [](osrm::engine::traffic_proto::TrafficPacketMsg &decoded) {
            if (decoded.user_id != 99 || decoded.speed_kmh != 42.0f)
                throw std::runtime_error("round-trip mismatch");
        });

    if (parsed != 1)
    {
        std::cerr << "TrafficBatch protobuf round-trip failed" << std::endl;
        return 1;
    }

    std::cout << "OK: TrafficBatch protobuf encode/decode (" << wire.size() << " bytes)" << std::endl;
    return 0;
}
