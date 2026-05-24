#!/bin/sh
set -e

cd "$(dirname "$0")/.."

mkdir -p build data backend/certs

# if [ ! -f backend/certs/cert.pem ]; then
#     echo "[setup] generating TLS certificate..."
#     openssl req -x509 -newkey rsa:2048 -nodes \
#         -keyout backend/certs/key.pem \
#         -out    backend/certs/cert.pem \
#         -days 365 \
#         -subj "/CN=localhost"
# fi

cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS_SHARED=ON \
    -DBUILD_TESTS_BACKEND=ON \
    -DBUILD_TESTS_TURN=ON \
    -DBUILD_TESTS_CLIENT=ON

cmake --build .
echo "[build] done"