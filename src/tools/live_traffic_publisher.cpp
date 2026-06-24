/**
 * UDP client: sends protobuf traffic.TrafficBatch to OSRM live traffic port.
 *
 * Usage: ./live_traffic_publisher <host> <port> <user_id> <lat> <lon> <speed_kmh> [bearing]
 */

#include "engine/traffic_protobuf.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

int main(int argc, char *argv[])
{
    if (argc < 7)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <host> <port> <user_id> <lat> <lon> <speed_kmh> [bearing_deg]" << std::endl;
        std::cerr << "Example: " << argv[0] << " localhost 9900 42 47.01 28.86 25.5 90" << std::endl;
        return 1;
    }

    const char *host = argv[1];
    const uint16_t port = static_cast<uint16_t>(std::stoul(argv[2]));
    osrm::engine::traffic_proto::TrafficPacketMsg packet;
    packet.user_id = std::stoull(argv[3]);
    packet.latitude = std::stod(argv[4]);
    packet.longitude = std::stod(argv[5]);
    packet.speed_kmh = std::stof(argv[6]);
    packet.bearing_deg = (argc > 7) ? std::stof(argv[7]) : 0.0f;
    packet.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count();

    const std::string payload = osrm::engine::traffic_proto::encodeTrafficBatch(packet);

    const int sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        std::cerr << "Error creating socket" << std::endl;
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0)
    {
        std::cerr << "Invalid host: " << host << std::endl;
        ::close(sockfd);
        return 1;
    }

    const ssize_t bytes_sent = ::sendto(sockfd,
                                        payload.data(),
                                        payload.size(),
                                        0,
                                        reinterpret_cast<sockaddr *>(&server_addr),
                                        sizeof(server_addr));
    ::close(sockfd);

    if (bytes_sent != static_cast<ssize_t>(payload.size()))
    {
        std::cerr << "Error sending TrafficBatch (" << bytes_sent << " bytes)" << std::endl;
        return 1;
    }

    std::cout << "TrafficBatch sent (" << payload.size() << " bytes protobuf)" << std::endl;
    return 0;
}
