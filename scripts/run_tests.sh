#!/bin/sh
set -e

cd "$(dirname "$0")/../build"

echo "=== test_crypto ==="
./tests/test_crypto

echo "=== test_backend ==="
./tests/test_backend

echo "=== test_turn ==="
./tests/test_turn

echo "=== test_client ==="
./tests/test_client

echo "=== all tests passed ==="