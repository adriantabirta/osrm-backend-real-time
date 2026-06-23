// include/engine/traffic_updater.hpp
#pragma once

#include "engine/live_data_store.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <thread>

namespace osrm::engine {

// UDP packet layout from GPS clients.
// edge_id is resolved server-side from lat/lon via map-matching.
// Total: 8+8+8+4+4+8 = 40 bytes
#pragma pack(push, 1)
struct TrafficPacket {
    uint64_t user_id;       // GPS device / user identifier
    double latitude;        // WGS84
    double longitude;       // WGS84
    float speed_kmh;        // measured speed
    float bearing_deg;      // direction 0-360 (0 = North)
    int64_t timestamp_ms;   // Unix timestamp in milliseconds
};
#pragma pack(pop)

static_assert(sizeof(TrafficPacket) == 40,
              "TrafficPacket size mismatch — verify alignment with UDP source");

class TrafficUpdater {
  public:
    explicit TrafficUpdater(uint16_t port = 9900,
                            int stale_seconds = 120,
                            float min_speed_kmh = 1.0f)
        : port_(port), stale_seconds_(stale_seconds), min_speed_kmh_(min_speed_kmh), sockfd_(-1),
          running_(false)
    {
    }

    ~TrafficUpdater() { stop(); }

    void start()
    {
        if (running_)
            return;

        sockfd_ = createSocket();
        running_ = true;
        thread_ = std::thread([this]() { receiveLoop(); });
    }

    void stop()
    {
        running_ = false;
        if (sockfd_ >= 0)
        {
            ::close(sockfd_);
            sockfd_ = -1;
        }
        if (thread_.joinable())
            thread_.join();
    }

    int staleSeconds() const { return stale_seconds_; }

    uint64_t packetsReceived() const { return packets_received_.load(); }
    uint64_t packetsDropped() const { return packets_dropped_.load(); }

  private:
    int createSocket()
    {
        int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0)
            throw std::runtime_error("TrafficUpdater: cannot create UDP socket");

        int opt = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        int rcvbuf = 4 * 1024 * 1024;
        ::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port_);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
        {
            ::close(fd);
            throw std::runtime_error("TrafficUpdater: bind() failed on port " +
                                     std::to_string(port_));
        }

        return fd;
    }

    void receiveLoop()
    {
        TrafficPacket pkt;
        sockaddr_in sender{};
        socklen_t sender_len = sizeof(sender);

        while (running_)
        {
            ssize_t bytes = ::recvfrom(sockfd_,
                                       &pkt,
                                       sizeof(pkt),
                                       0,
                                       reinterpret_cast<sockaddr *>(&sender),
                                       &sender_len);

            if (bytes < 0)
                continue;

            if (bytes != sizeof(TrafficPacket))
            {
                ++packets_dropped_;
                continue;
            }

            processPacket(pkt);
            ++packets_received_;
        }
    }

    void processPacket(const TrafficPacket &pkt)
    {
        if (pkt.speed_kmh < min_speed_kmh_ || pkt.speed_kmh > 300.0f)
            return;
        if (pkt.latitude < -90.0 || pkt.latitude > 90.0)
            return;
        if (pkt.longitude < -180.0 || pkt.longitude > 180.0)
            return;

        const auto now_ms = currentTimeMs();
        if (now_ms - pkt.timestamp_ms > static_cast<int64_t>(stale_seconds_) * 1000)
            return;

        TrafficSample sample;
        sample.user_id = pkt.user_id;
        sample.latitude = pkt.latitude;
        sample.longitude = pkt.longitude;
        sample.speed_kmh = pkt.speed_kmh;
        sample.bearing_deg = pkt.bearing_deg;
        sample.timestamp_ms = pkt.timestamp_ms;

        LiveDataStore::instance().addSample(std::move(sample));
    }

    static int64_t currentTimeMs()
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }

    uint16_t port_;
    int stale_seconds_;
    float min_speed_kmh_;
    int sockfd_;
    std::atomic<bool> running_;
    std::thread thread_;

    std::atomic<uint64_t> packets_received_{0};
    std::atomic<uint64_t> packets_dropped_{0};
};

} // namespace osrm::engine
