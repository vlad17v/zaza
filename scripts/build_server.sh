#!/bin/sh
set -e

BUILD_TYPE="${1:-Release}"

cd "$(dirname "$0")/.."

mkdir -p build data backend/certs

cd build

cmake .. -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

cmake --build . --target backend_server turn_server

echo "[build] server done (${BUILD_TYPE})"