#!/bin/sh

# Build
cd build
cmake ..
cmake --build .

# Install binaries

#sudo cmake --build . --target install

sudo install ./osrm-realtime-routing /usr/bin/
sudo install ./osrm-nearest /usr/bin/