#!/bin/bash
# Quick build script for Ring Tree C++

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}╔═══════════════════════════════════════════════════╗${NC}"
echo -e "${YELLOW}║  Ring Tree C++.                                   ║${NC}"
echo -e "${YELLOW}╚═══════════════════════════════════════════════════╝${NC}\n"

# Detect OS
OS_TYPE=$(uname -s)
echo "Detected OS: $OS_TYPE"

# Check dependencies
echo -e "\n${YELLOW}Checking dependencies...${NC}"

if ! command -v cmake &> /dev/null; then
    echo -e "${RED}✗ CMake not found. Please install CMake 3.20+${NC}"
    exit 1
fi

if ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then
    echo -e "${RED}✗ No C++ compiler found. Please install GCC or Clang${NC}"
    exit 1
fi

COMPILER=$(g++ --version 2>/dev/null || clang++ --version)
echo -e "${GREEN}✓ C++ Compiler: $(echo "$COMPILER" | head -n1)${NC}"

# Create build directory
echo -e "\n${YELLOW}Setting up build directory...${NC}"
mkdir -p build
cd build

# Configure CMake
echo -e "\n${YELLOW}Configuring CMake (Release mode with optimizations)...${NC}"
cmake -DCMAKE_BUILD_TYPE=Release ..

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ CMake configuration successful${NC}"
else
    echo -e "${RED}✗ CMake configuration failed${NC}"
    exit 1
fi

# Build
echo -e "\n${YELLOW}Building...${NC}"
NUM_JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
make -j $NUM_JOBS

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Build successful!${NC}"
else
    echo -e "${RED}✗ Build failed${NC}"
    exit 1
fi

# Run tests
echo -e "\n${YELLOW}Running tests...${NC}"
ctest --output-on-failure

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ All tests passed!${NC}"
else
    echo -e "${RED}✗ Some tests failed${NC}"
    exit 1
fi

# Summary
echo -e "\n${GREEN}╔═══════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  ✓ Build Complete!                               ║${NC}"
echo -e "${GREEN}╠═══════════════════════════════════════════════════╣${NC}"
echo -e "${GREEN}║  Executables:                                     ║${NC}"
echo -e "${GREEN}║    • ring_tree_test   (tests + benchmarks)        ║${NC}"

if [ -f "ring_tree_bench" ]; then
    echo -e "${GREEN}║    • ring_tree_bench  (HFT multi-threaded)        ║${NC}"
fi

echo -e "${GREEN}║                                                   ║${NC}"
echo -e "${GREEN}║  Run benchmarks:                                  ║${NC}"
echo -e "${GREEN}║    ./ring_tree_test     (basic + single-threaded) ║${NC}"

if [ -f "ring_tree_bench" ]; then
    echo -e "${GREEN}║    ./ring_tree_bench    (multi-threaded HFT)      ║${NC}"
fi

echo -e "${GREEN}║                                                   ║${NC}"
echo -e "${GREEN}║  Documentation:                                   ║${NC}"
echo -e "${GREEN}║    • README.md          (architecture overview)   ║${NC}"
echo -e "${GREEN}║    • DESIGN.md          (HFT optimizations)       ║${NC}"
echo -e "${GREEN}║    • BUILD.md           (build guide)             ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════════════════╝${NC}\n"
