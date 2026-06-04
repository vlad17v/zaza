#!/bin/sh
set -e

cd "$(dirname "$0")/.."

mkdir -p build data backend/certs

cd build
cmake .. -DCMAKE_BUILD_TYPE=Release

cmake --build . --target backend_server turn_server
echo "[build] server done"
