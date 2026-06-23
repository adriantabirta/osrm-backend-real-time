#include "engine/live_data_store.hpp"
#include "engine/traffic_updater.hpp"

#include <boost/test/unit_test.hpp>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <random>
#include <thread>
#include <vector>

using namespace osrm::engine;

namespace
{

int64_t nowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

uint16_t findFreeUdpPort()
{
    const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    BOOST_REQUIRE(fd >= 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    BOOST_REQUIRE(::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0);

    socklen_t len = sizeof(addr);
    BOOST_REQUIRE(::getsockname(fd, reinterpret_cast<sockaddr *>(&addr), &len) == 0);
    ::close(fd);
    return ntohs(addr.sin_port);
}

void sendTrafficPacket(uint16_t port,
                       uint64_t user_id,
                       double lat,
                       double lon,
                       float speed_kmh,
                       float bearing_deg)
{
    TrafficPacket pkt{};
    pkt.user_id = user_id;
    pkt.latitude = lat;
    pkt.longitude = lon;
    pkt.speed_kmh = speed_kmh;
    pkt.bearing_deg = bearing_deg;
    pkt.timestamp_ms = nowMs();

    const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    BOOST_REQUIRE(fd >= 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    const ssize_t bytes = ::sendto(fd,
                                   &pkt,
                                   sizeof(pkt),
                                   0,
                                   reinterpret_cast<sockaddr *>(&addr),
                                   sizeof(addr));
    ::close(fd);
    BOOST_REQUIRE_EQUAL(bytes, static_cast<ssize_t>(sizeof(pkt)));
}

} // namespace

BOOST_AUTO_TEST_SUITE(live_traffic_concurrency)

BOOST_AUTO_TEST_CASE(live_data_store_handles_parallel_readers_and_writers)
{
    LiveDataStore::instance().clear();
    LiveDataStore::instance().setStaleSeconds(120);

    std::atomic<bool> stop{false};
    std::atomic<uint64_t> writes{0};
    std::atomic<uint64_t> reads{0};

    constexpr int writer_threads = 8;
    constexpr int reader_threads = 8;
    constexpr auto test_duration = std::chrono::seconds(3);

    std::vector<std::thread> threads;
    threads.reserve(writer_threads + reader_threads);

    for (int t = 0; t < writer_threads; ++t)
    {
        threads.emplace_back([&, base_user = static_cast<uint64_t>(t) * 1'000]() {
            std::mt19937 rng(static_cast<unsigned>(base_user + 17));
            std::uniform_real_distribution<double> speed_dist(5.0, 120.0);
            std::uniform_real_distribution<double> coord_jitter(-0.01, 0.01);

            while (!stop.load(std::memory_order_relaxed))
            {
                TrafficSample sample;
                sample.user_id = base_user + (writes.load(std::memory_order_relaxed) % 64);
                sample.latitude = 52.5 + coord_jitter(rng);
                sample.longitude = 13.4 + coord_jitter(rng);
                sample.speed_kmh = static_cast<float>(speed_dist(rng));
                sample.bearing_deg = static_cast<float>((writes.load() * 17) % 360);
                sample.timestamp_ms = nowMs();

                LiveDataStore::instance().addSample(sample);

                TrafficEdgeData segment;
                segment.speed_kmh = speed_dist(rng);
                segment.weight_factor = 1.0;
                segment.sample_count = 1;
                segment.timestamp_ms = nowMs();
                segment.source = "live";
                LiveDataStore::instance().update(
                    static_cast<uint64_t>(writes.load(std::memory_order_relaxed) % 512),
                    segment);

                writes.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (int t = 0; t < reader_threads; ++t)
    {
        threads.emplace_back([&]() {
            while (!stop.load(std::memory_order_relaxed))
            {
                (void)LiveDataStore::instance().pendingSampleCount();
                (void)LiveDataStore::instance().size();
                (void)LiveDataStore::instance().get(static_cast<uint64_t>(reads.load() % 512));
                auto pending = LiveDataStore::instance().takePendingSamples();
                for (auto &sample : pending)
                    LiveDataStore::instance().addSample(std::move(sample));
                (void)LiveDataStore::instance().getAllSegments();
                LiveDataStore::instance().removeStaleSegments(120);
                reads.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    std::this_thread::sleep_for(test_duration);
    stop.store(true, std::memory_order_relaxed);
    for (auto &thread : threads)
        thread.join();

    BOOST_TEST(writes.load() > 500);
    BOOST_TEST(reads.load() > 50);

    LiveDataStore::instance().clear();
}

BOOST_AUTO_TEST_CASE(traffic_updater_handles_burst_udp_load)
{
    LiveDataStore::instance().clear();

    const auto port = findFreeUdpPort();
    TrafficUpdater updater(port, 120, 1.0f);
    updater.start();

    std::atomic<bool> stop{false};
    std::atomic<uint64_t> packets_sent{0};

    constexpr int sender_threads = 6;
    constexpr auto test_duration = std::chrono::seconds(4);
    constexpr int max_rate_per_second = 100;

    std::vector<std::thread> senders;
    senders.reserve(sender_threads);

    for (int t = 0; t < sender_threads; ++t)
    {
        senders.emplace_back([&, thread_id = t]() {
            std::mt19937 rng(static_cast<unsigned>(thread_id + 99));
            std::uniform_int_distribution<int> sleep_us(0, 25'000);
            std::uniform_real_distribution<double> speed_dist(1.0, 130.0);
            std::uniform_real_distribution<double> coord_jitter(-0.02, 0.02);

            auto window_start = std::chrono::steady_clock::now();
            int sent_in_window = 0;

            while (!stop.load(std::memory_order_relaxed))
            {
                const auto now = std::chrono::steady_clock::now();
                if (now - window_start >= std::chrono::seconds(1))
                {
                    window_start = now;
                    sent_in_window = 0;
                }

                if (sent_in_window >= max_rate_per_second / sender_threads)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    continue;
                }

                const uint64_t user_id = static_cast<uint64_t>(thread_id) * 10'000 +
                                         (packets_sent.load(std::memory_order_relaxed) % 500);
                sendTrafficPacket(port,
                                  user_id,
                                  52.5 + coord_jitter(rng),
                                  13.4 + coord_jitter(rng),
                                  static_cast<float>(speed_dist(rng)),
                                  static_cast<float>((user_id * 13) % 360));

                packets_sent.fetch_add(1, std::memory_order_relaxed);
                sent_in_window++;
                std::this_thread::sleep_for(std::chrono::microseconds(sleep_us(rng)));
            }
        });
    }

    std::this_thread::sleep_for(test_duration);
    stop.store(true, std::memory_order_relaxed);
    for (auto &sender : senders)
        sender.join();

    updater.stop();

    const auto received = updater.packetsReceived();
    const auto dropped = updater.packetsDropped();

    BOOST_TEST(packets_sent.load() > 100);
    BOOST_TEST(received > 50);
    BOOST_TEST(received + dropped >= packets_sent.load() / 2);

    LiveDataStore::instance().clear();
}

BOOST_AUTO_TEST_SUITE_END()
