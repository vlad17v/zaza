#!/bin/sh
set -e

BUILD_TYPE="${1:-Release}"

cd "$(dirname "$0")/.."

mkdir -p build data backend/certs

cd build

cmake .. \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DBUILD_TESTS_SHARED=ON \
    -DBUILD_TESTS_BACKEND=ON \
    -DBUILD_TESTS_TURN=ON \
    -DBUILD_TESTS_CLIENT=ON

cmake --build .

echo "[build] all done (${BUILD_TYPE})"