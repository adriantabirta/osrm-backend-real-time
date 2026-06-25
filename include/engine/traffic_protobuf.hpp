#ifndef ENGINE_TRAFFIC_PROTOBUF_HPP
#define ENGINE_TRAFFIC_PROTOBUF_HPP

#include <protozero/pbf_reader.hpp>
#include <protozero/pbf_writer.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace osrm::engine::traffic_proto
{

// Mirrors proto/traffic.proto (proto3, decoded via protozero — no libprotobuf runtime).
struct TrafficPacketMsg {
    uint64_t user_id = 0;
    double latitude = 0.0;
    double longitude = 0.0;
    float speed_kmh = 0.0f;
    float bearing_deg = 0.0f;
    int64_t timestamp_ms = 0;
};

enum class TrafficPacketField : protozero::pbf_tag_type {
    user_id = 1,
    latitude = 2,
    longitude = 3,
    speed_kmh = 4,
    bearing_deg = 5,
    timestamp_ms = 6
};

enum class TrafficBatchField : protozero::pbf_tag_type {
    packets = 1
};

inline bool parseTrafficPacket(protozero::pbf_reader &reader, TrafficPacketMsg &out)
{
    bool has_coords = false;

    while (reader.next())
    {
        switch (reader.tag())
        {
        case static_cast<protozero::pbf_tag_type>(TrafficPacketField::user_id):
            out.user_id = reader.get_uint64();
            break;
        case static_cast<protozero::pbf_tag_type>(TrafficPacketField::latitude):
            out.latitude = reader.get_double();
            has_coords = true;
            break;
        case static_cast<protozero::pbf_tag_type>(TrafficPacketField::longitude):
            out.longitude = reader.get_double();
            has_coords = true;
            break;
        case static_cast<protozero::pbf_tag_type>(TrafficPacketField::speed_kmh):
            out.speed_kmh = reader.get_float();
            break;
        case static_cast<protozero::pbf_tag_type>(TrafficPacketField::bearing_deg):
            out.bearing_deg = reader.get_float();
            break;
        case static_cast<protozero::pbf_tag_type>(TrafficPacketField::timestamp_ms):
            out.timestamp_ms = static_cast<int64_t>(reader.get_int64());
            break;
        default:
            reader.skip();
            break;
        }
    }

    return has_coords;
}

inline std::size_t parseTrafficBatch(const char *data,
                                     std::size_t size,
                                     const std::function<void(TrafficPacketMsg &)> &consumer)
{
    std::size_t parsed = 0;

    try
    {
        protozero::pbf_reader batch(data, size);
        while (batch.next())
        {
            if (batch.tag() != static_cast<protozero::pbf_tag_type>(TrafficBatchField::packets))
            {
                batch.skip();
                continue;
            }

            protozero::pbf_reader packet_reader = batch.get_message();
            TrafficPacketMsg packet;
            if (!parseTrafficPacket(packet_reader, packet))
                continue;

            consumer(packet);
            ++parsed;
            fprintf(stderr, "[PROTO] packet parsed: user_id=%llu speed=%.1f\n", (unsigned long long)packet.user_id, packet.speed_kmh);
        }
    }
    catch (const protozero::exception &e)
    {
        fprintf(stderr, "[PROTO] parse error: %s (size=%zu)\n", e.what(), size);
        return 0;
    }

    return parsed;
}

inline std::string encodeTrafficBatch(const std::vector<TrafficPacketMsg> &packets)
{
    std::string buffer;
    protozero::pbf_writer batch_writer(buffer);

    for (const auto &packet : packets)
    {
        std::string submessage;
        protozero::pbf_writer writer(submessage);
        writer.add_uint64(static_cast<uint32_t>(TrafficPacketField::user_id), packet.user_id);
        writer.add_double(static_cast<uint32_t>(TrafficPacketField::latitude), packet.latitude);
        writer.add_double(static_cast<uint32_t>(TrafficPacketField::longitude), packet.longitude);
        writer.add_float(static_cast<uint32_t>(TrafficPacketField::speed_kmh), packet.speed_kmh);
        writer.add_float(static_cast<uint32_t>(TrafficPacketField::bearing_deg), packet.bearing_deg);
        writer.add_int64(static_cast<uint32_t>(TrafficPacketField::timestamp_ms), packet.timestamp_ms);
        batch_writer.add_message(static_cast<uint32_t>(TrafficBatchField::packets), submessage);
    }

    return buffer;
}

inline std::string encodeTrafficBatch(const TrafficPacketMsg &packet)
{
    return encodeTrafficBatch(std::vector<TrafficPacketMsg>{packet});
}

} // namespace osrm::engine::traffic_proto

#endif
