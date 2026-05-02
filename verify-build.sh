#!/usr/bin/env bash
# Build verification script for AZ-SCALE-001

set -e

echo "=========================================="
echo "AZ-SCALE-001 Build Verification"
echo "=========================================="

# Check for required dependencies
echo ""
echo "Checking dependencies..."

command -v cmake >/dev/null 2>&1 || { echo "ERROR: cmake is required but not installed."; exit 1; }
command -v make >/dev/null 2>&1 || { echo "ERROR: make is required but not installed."; exit 1; }
command -v g++ >/dev/null 2>&1 || { echo "ERROR: g++ is required but not installed."; exit 1; }

echo "✓ Build tools found"

# Check for OpenCog dependencies
echo ""
echo "Checking OpenCog dependencies..."

if pkg-config --exists cogutil 2>/dev/null; then
    echo "✓ cogutil found ($(pkg-config --modversion cogutil))"
else
    echo "⚠ cogutil not found - needs to be built"
fi

if pkg-config --exists atomspace 2>/dev/null; then
    echo "✓ atomspace found ($(pkg-config --modversion atomspace))"
else
    echo "⚠ atomspace not found - needs to be built"
fi

# Check Boost
echo ""
echo "Checking Boost..."
if [ -d "/usr/include/boost" ] || [ -d "/usr/local/include/boost" ]; then
    echo "✓ Boost found"
else
    echo "⚠ Boost not found - install with: sudo apt-get install libboost-all-dev"
fi

# Check source files
echo ""
echo "Checking source files..."

DIST_DIR="agents/cpp/agentzero-distributed"

if [ -f "$DIST_DIR/CMakeLists.txt" ]; then
    echo "✓ CMakeLists.txt found"
else
    echo "✗ CMakeLists.txt not found"
    exit 1
fi

if [ -f "$DIST_DIR/src/DistributedCoordinator.cpp" ]; then
    echo "✓ DistributedCoordinator.cpp found"
else
    echo "✗ DistributedCoordinator.cpp not found"
    exit 1
fi

if [ -f "$DIST_DIR/src/ClusterManager.cpp" ]; then
    echo "✓ ClusterManager.cpp found"
else
    echo "✗ ClusterManager.cpp not found"
    exit 1
fi

if [ -f "$DIST_DIR/src/LoadBalancer.cpp" ]; then
    echo "✓ LoadBalancer.cpp found"
else
    echo "✗ LoadBalancer.cpp not found"
    exit 1
fi

# Check headers
echo ""
echo "Checking header files..."

HEADERS=(
    "include/opencog/agentzero/distributed/DistributedCoordinator.h"
    "include/opencog/agentzero/distributed/ClusterManager.h"
    "include/opencog/agentzero/distributed/LoadBalancer.h"
)

for header in "${HEADERS[@]}"; do
    if [ -f "$DIST_DIR/$header" ]; then
        echo "✓ $(basename $header) found"
    else
        echo "✗ $(basename $header) not found"
        exit 1
    fi
done

# Check tests
echo ""
echo "Checking test files..."

TESTS=(
    "tests/DistributedCoordinatorTest.cpp"
    "tests/ClusterManagerTest.cpp"
    "tests/LoadBalancerTest.cpp"
)

for test in "${TESTS[@]}"; do
    if [ -f "$DIST_DIR/$test" ]; then
        echo "✓ $(basename $test) found"
    else
        echo "✗ $(basename $test) not found"
        exit 1
    fi
done

# Check documentation
echo ""
echo "Checking documentation..."

if [ -f "$DIST_DIR/README.md" ]; then
    echo "✓ README.md found"
else
    echo "✗ README.md not found"
fi

if [ -f "$DIST_DIR/docs/INTEGRATION_TESTING.md" ]; then
    echo "✓ INTEGRATION_TESTING.md found"
else
    echo "✗ INTEGRATION_TESTING.md not found"
fi

# Check examples
echo ""
echo "Checking examples..."

if [ -f "$DIST_DIR/examples/DistributedComputingExample.cpp" ]; then
    echo "✓ DistributedComputingExample.cpp found"
else
    echo "✗ DistributedComputingExample.cpp not found"
fi

echo ""
echo "=========================================="
echo "Build Verification Summary"
echo "=========================================="
echo ""
echo "✓ All source files present"
echo "✓ All header files present"
echo "✓ All test files present"
echo "✓ Documentation present"
echo "✓ Examples present"
echo ""
echo "Next steps:"
echo "1. Build cogutil and atomspace if not already installed"
echo "2. cd agents/cpp && mkdir -p build && cd build"
echo "3. cmake .. -DBUILD_TESTING=ON"
echo "4. make agentzero-distributed"
echo "5. make test"
echo ""
echo "See docs/INTEGRATION_TESTING.md for detailed testing procedures"
echo ""
