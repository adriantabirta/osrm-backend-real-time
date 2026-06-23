/**
 * Simple UDP client that sends live traffic data to OSRM
 * 
 * Usage: ./live_traffic_publisher <server_ip> <server_port> <edge_id> <lat> <lon> <speed_kmh>
 * Example: ./live_traffic_publisher localhost 9000 12345 52.5 13.4 25.5
 */

#include <iostream>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

// Must match the struct in traffic_updater.hpp
#pragma pack(push, 1)
struct TrafficPacket {
    uint64_t edge_id;       // ID-ul edge-ului în graful OSRM
    double   latitude;      // WGS84
    double   longitude;     // WGS84
    float    speed_kmh;     // viteza măsurată
    float    bearing_deg;   // direcție 0-360 (0 = Nord)
    int64_t  timestamp_ms;  // Unix timestamp în milisecunde
};
#pragma pack(pop)

static_assert(sizeof(TrafficPacket) == 40, "TrafficPacket size mismatch");

int main(int argc, char *argv[])
{
    if (argc < 7) {
        std::cerr << "Usage: " << argv[0] 
                  << " <server_ip> <server_port> <edge_id> <lat> <lon> <speed_kmh> [bearing_deg]"
                  << std::endl;
        std::cerr << "Example: " << argv[0] << " localhost 9000 12345 52.5 13.4 25.5 90"
                  << std::endl;
        return 1;
    }

    const char *server_ip = argv[1];
    uint16_t server_port = std::stoul(argv[2]);
    uint64_t edge_id = std::stoull(argv[3]);
    double latitude = std::stod(argv[4]);
    double longitude = std::stod(argv[5]);
    float speed_kmh = std::stof(argv[6]);
    float bearing_deg = (argc > 7) ? std::stof(argv[7]) : 0.0f;

    // Create UDP socket
    int sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Error creating socket" << std::endl;
        return 1;
    }

    // Setup server address
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    
    if (::inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid IP address: " << server_ip << std::endl;
        ::close(sockfd);
        return 1;
    }

    // Create and send packet
    TrafficPacket pkt;
    pkt.edge_id = edge_id;
    pkt.latitude = latitude;
    pkt.longitude = longitude;
    pkt.speed_kmh = speed_kmh;
    pkt.bearing_deg = bearing_deg;
    
    // Set current time
    using namespace std::chrono;
    pkt.timestamp_ms = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()
    ).count();

    ssize_t bytes_sent = ::sendto(
        sockfd,
        &pkt, sizeof(pkt),
        0,
        reinterpret_cast<sockaddr*>(&server_addr),
        sizeof(server_addr)
    );

    if (bytes_sent < 0) {
        std::cerr << "Error sending packet" << std::endl;
        ::close(sockfd);
        return 1;
    }

    if (bytes_sent != sizeof(TrafficPacket)) {
        std::cerr << "Partial send: " << bytes_sent << " of " 
                  << sizeof(TrafficPacket) << " bytes" << std::endl;
        ::close(sockfd);
        return 1;
    }

    std::cout << "✓ Traffic packet sent successfully:" << std::endl;
    std::cout << "  Edge ID: " << edge_id << std::endl;
    std::cout << "  Location: " << latitude << ", " << longitude << std::endl;
    std::cout << "  Speed: " << speed_kmh << " km/h" << std::endl;
    std::cout << "  Bearing: " << bearing_deg << "°" << std::endl;

    ::close(sockfd);
    return 0;
}
