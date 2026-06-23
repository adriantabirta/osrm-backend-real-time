// Standalone check for the live traffic UDP wire format.
#include <cstdint>
#include <cstring>
#include <iostream>

#pragma pack(push, 1)
struct TrafficPacket {
    uint64_t user_id;
    double latitude;
    double longitude;
    float speed_kmh;
    float bearing_deg;
    int64_t timestamp_ms;
};
#pragma pack(pop)

int main()
{
    static_assert(sizeof(TrafficPacket) == 40, "TrafficPacket must be 40 bytes");

    TrafficPacket pkt{};
    pkt.user_id = 99;
    pkt.latitude = 44.4;
    pkt.longitude = 26.1;
    pkt.speed_kmh = 42.0f;
    pkt.bearing_deg = 180.0f;
    pkt.timestamp_ms = 1'700'000'000'000;

    unsigned char wire[40];
    std::memcpy(wire, &pkt, sizeof(pkt));

    TrafficPacket decoded{};
    std::memcpy(&decoded, wire, sizeof(decoded));

    if (decoded.user_id != 99 || decoded.speed_kmh != 42.0f)
    {
        std::cerr << "UDP packet round-trip failed" << std::endl;
        return 1;
    }

    std::cout << "OK: TrafficPacket wire format (40 bytes, user_id + GPS + speed)" << std::endl;
    return 0;
}
