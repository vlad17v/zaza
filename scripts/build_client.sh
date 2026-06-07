#!/bin/sh
set -e

BUILD_TYPE="${1:-Release}"

cd "$(dirname "$0")/.."

mkdir -p build

cd build

cmake .. -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

cmake --build . --target client_cli

echo "[build] client done (${BUILD_TYPE})"