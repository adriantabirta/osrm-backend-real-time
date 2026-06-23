FROM debian:bookworm-slim AS builder

ARG BUILD_CONCURRENCY

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      ca-certificates cmake make git gcc g++ pkg-config wget \
      libbz2-dev libexpat1-dev libzip-dev liblua5.4-dev \
      libboost-all-dev libtbb-dev zlib1g-dev && \
    rm -rf /var/lib/apt/lists/*

COPY . /src
WORKDIR /src

RUN NPROC=${BUILD_CONCURRENCY:-$(nproc)} && \
    mkdir -p build && cd build && \
    cmake .. \
      -DCMAKE_BUILD_TYPE=Release \
      -DENABLE_NODE_BINDINGS=OFF \
      -DENABLE_LTO=On \
      -DBUILD_TOOLS=Off && \
    cmake --build . --target install -j"${NPROC}" && \
    cp -r ../profiles/* /opt/osrm-profiles && \
    strip /usr/local/bin/* 2>/dev/null || true

FROM debian:bookworm-slim AS runstage

COPY --from=builder /usr/local /usr/local
COPY --from=builder /opt/osrm-profiles /opt
COPY docker/docker-entrypoint.sh /usr/local/bin/docker-entrypoint.sh

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      libboost-program-options1.74.0 libboost-regex1.74.0 \
      libboost-date-time1.74.0 libboost-chrono1.74.0 \
      libboost-filesystem1.74.0 libboost-iostreams1.74.0 \
      libboost-thread1.74.0 libtbb12 libexpat1 liblua5.4-0 && \
    chmod +x /usr/local/bin/docker-entrypoint.sh && \
    rm -rf /var/lib/apt/lists/* && \
    ldconfig /usr/local/lib

RUN osrm-routed --help && osrm-extract --help

WORKDIR /opt

ENV LIVE_DATA_UDP_PORT=9900
ENV OSRM_PORT=7070
ENV LIVE_DATA_STALE_SECONDS=120
ENV OSRM_ALGORITHM=mld

EXPOSE 7070
EXPOSE 9900/udp

ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]
