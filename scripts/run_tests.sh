#!/bin/sh
set -e

cd "$(dirname "$0")/.."

if [ ! -f build/tests/test_crypto ] || \
   [ ! -f build/tests/test_backend ] || \
   [ ! -f build/tests/test_turn ] || \
   [ ! -f build/tests/test_client ]; then

    echo "[tests] building tests..."
    mkdir -p build
    cd build
    cmake .. \
        -DCMAKE_BUILD_TYPE=Debug \
        -DBUILD_TESTS_SHARED=ON \
        -DBUILD_TESTS_BACKEND=ON \
        -DBUILD_TESTS_TURN=ON \
        -DBUILD_TESTS_CLIENT=ON
    cmake --build . --target test_crypto test_backend test_turn test_client
    cd ..
fi

cd build

PASS=0
FAIL=0

run_test() {
    NAME="$1"
    BIN="$2"
    echo ""
    echo "=== ${NAME} ==="
    if "${BIN}"; then
        PASS=$((PASS + 1))
    else
        echo "[FAIL] ${NAME}"
        FAIL=$((FAIL + 1))
    fi
}

run_test "test_crypto"  ./tests/test_crypto
run_test "test_backend" ./tests/test_backend
run_test "test_turn"    ./tests/test_turn
run_test "test_client"  ./tests/test_client

echo ""
echo "================================"
echo "Passed: ${PASS} / $((PASS + FAIL))"

if [ "${FAIL}" -gt 0 ]; then
    echo "FAILED: ${FAIL} test(s)"
    exit 1
else
    echo "=== all tests passed ==="
fi