// include/engine/traffic_updater.hpp
#pragma once

#include "engine/live_data_store.hpp"
#include "engine/traffic_protobuf.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <thread>

namespace osrm::engine {

// UDP payload: protobuf traffic.TrafficBatch (see proto/traffic.proto).
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
    uint64_t batchesReceived() const { return batches_received_.load(); }

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
        std::array<char, 65536> buffer{};
        sockaddr_in sender{};
        socklen_t sender_len = sizeof(sender);

        while (running_)
        {
            ssize_t bytes = ::recvfrom(sockfd_,
                                       buffer.data(),
                                       buffer.size(),
                                       0,
                                       reinterpret_cast<sockaddr *>(&sender),
                                       &sender_len);

            if (bytes < 0)
                continue;

            if (bytes == 0)
            {
                ++packets_dropped_;
                continue;
            }

            const auto parsed = traffic_proto::parseTrafficBatch(
                buffer.data(),
                static_cast<std::size_t>(bytes),
                [this](traffic_proto::TrafficPacketMsg &packet) { processPacket(packet); });

            if (parsed == 0)
            {
                ++packets_dropped_;
                continue;
            }

            ++batches_received_;
            packets_received_.fetch_add(parsed, std::memory_order_relaxed);
        }
    }

    void processPacket(const traffic_proto::TrafficPacketMsg &pkt)
    {
        if (pkt.speed_kmh < min_speed_kmh_ || pkt.speed_kmh > 300.0f)
            return;
        if (pkt.latitude < -90.0 || pkt.latitude > 90.0)
            return;
        if (pkt.longitude < -180.0 || pkt.longitude > 180.0)
            return;

        const auto now_ms = currentTimeMs();
        if (pkt.timestamp_ms > 0 &&
            now_ms - pkt.timestamp_ms > static_cast<int64_t>(stale_seconds_) * 1000)
            return;

        TrafficSample sample;
        sample.user_id = pkt.user_id;
        sample.latitude = pkt.latitude;
        sample.longitude = pkt.longitude;
        sample.speed_kmh = pkt.speed_kmh;
        sample.bearing_deg = pkt.bearing_deg;
        sample.timestamp_ms = pkt.timestamp_ms > 0 ? pkt.timestamp_ms : now_ms;

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
    std::atomic<uint64_t> batches_received_{0};
};

} // namespace osrm::engine
