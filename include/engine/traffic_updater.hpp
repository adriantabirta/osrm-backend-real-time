// include/engine/traffic_updater.hpp
#pragma once

#include "engine/live_data_store.hpp"

#include <thread>
#include <atomic>
#include <cstdint>
#include <cstring>      // memcpy
#include <stdexcept>

// POSIX sockets (Linux/macOS)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace osrm::engine {

// ─── Layout binar al pachetului UDP ─────────────────────────────────────────
// Trebuie să fie identic cu ce trimite sursa (GPS device, server extern etc.)
// Total: 8+8+8+4+4+8 = 40 bytes
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

static_assert(sizeof(TrafficPacket) == 40,
    "TrafficPacket size mismatch — verifică alinierea cu sursa UDP");

// ─── TrafficUpdater ──────────────────────────────────────────────────────────
class TrafficUpdater {
public:
    explicit TrafficUpdater(uint16_t port = 9000, int stale_seconds = 120, float min_speed_kmh = 1.0f)
        : port_(port)
        , stale_seconds_(stale_seconds)
        , min_speed_kmh_(min_speed_kmh)
        , sockfd_(-1)
        , running_(false)
    {}

    ~TrafficUpdater() { stop(); }

    // Apelat la pornirea OSRM (din osrm.cpp sau main)
    void start() {
        if (running_) return;

        sockfd_ = createSocket();
        running_ = true;
        thread_  = std::thread([this]() { receiveLoop(); });
    }

    void stop() {
        running_ = false;
        if (sockfd_ >= 0) {
            ::close(sockfd_);
            sockfd_ = -1;
        }
        if (thread_.joinable()) thread_.join();
    }

    // Statistici — opțional, util pentru logging
    uint64_t packetsReceived() const { return packets_received_.load(); }
    uint64_t packetsDropped()  const { return packets_dropped_.load(); }

private:
    int createSocket() {
        int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0)
            throw std::runtime_error("TrafficUpdater: nu pot crea socket UDP");

        // Permite restart rapid fără "Address already in use"
        int opt = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        // Buffer de recepție mare — pachete GPS pot veni în rafale
        int rcvbuf = 4 * 1024 * 1024; // 4MB
        ::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

        // Timeout pe recv — ca să putem ieși din loop la stop()
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons(port_);
        addr.sin_addr.s_addr = INADDR_ANY;   // ascultă pe toate interfețele

        if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            ::close(fd);
            throw std::runtime_error(
                "TrafficUpdater: bind() pe portul " + std::to_string(port_) + " a eșuat");
        }

        return fd;
    }

    void receiveLoop() {
        TrafficPacket pkt;
        sockaddr_in   sender{};
        socklen_t     sender_len = sizeof(sender);

        while (running_) {
            ssize_t bytes = ::recvfrom(
                sockfd_,
                &pkt, sizeof(pkt),
                0,
                reinterpret_cast<sockaddr*>(&sender),
                &sender_len
            );

            if (bytes < 0) {
                // EAGAIN = timeout de 1s — normal, revenim la while (running_)
                continue;
            }

            if (bytes != sizeof(TrafficPacket)) {
                // Pachet malformat — ignorăm
                ++packets_dropped_;
                continue;
            }

            processPacket(pkt);
            ++packets_received_;
        }
    }

    void processPacket(const TrafficPacket& pkt) {
        // Validări de bază
        if (pkt.speed_kmh < min_speed_kmh_ || pkt.speed_kmh > 300.0f) return;
        if (pkt.latitude  < -90.0 || pkt.latitude  > 90.0)  return;
        if (pkt.longitude < -180.0|| pkt.longitude > 180.0) return;

        auto now_ms = currentTimeMs();
        auto age_ms = now_ms - pkt.timestamp_ms;

        // Dacă pachetul e mai vechi de stale_seconds_, îl ignorăm
        if (age_ms > static_cast<int64_t>(stale_seconds_) * 1000) return;

        TrafficEdgeData data;
        data.speed_kmh     = static_cast<double>(pkt.speed_kmh);
        data.source        = "live";
        data.bearing_deg   = static_cast<double>(pkt.bearing_deg);
        data.timestamp_ms  = pkt.timestamp_ms;
        data.latitude      = pkt.latitude;
        data.longitude     = pkt.longitude;

        // weight_factor: cu cât e mai mic speed-ul față de limita tipică,
        // cu atât creștem costul edge-ului în rutare
        // Exemplu: speed=15 pe un drum de 50 → factor=50/15≈3.3 (mai lent)
        constexpr double REFERENCE_SPEED = 50.0;
        if (data.speed_kmh > 0.5) {
            data.weight_factor = REFERENCE_SPEED / data.speed_kmh;
            // Cap la 10x — evităm să blocăm complet un segment
            if (data.weight_factor > 10.0) data.weight_factor = 10.0;
        } else {
            data.weight_factor = 10.0; // aproape blocat
        }

        LiveDataStore::instance().update(pkt.edge_id, std::move(data));
    }

    static int64_t currentTimeMs() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()
        ).count();
    }

    uint16_t          port_;
    int               stale_seconds_;
    float             min_speed_kmh_;
    int               sockfd_;
    std::atomic<bool> running_;
    std::thread       thread_;

    std::atomic<uint64_t> packets_received_{0};
    std::atomic<uint64_t> packets_dropped_{0};
};

} // namespace osrm::engine
