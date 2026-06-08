cmake_minimum_required(VERSION 3.20)
project(RingTreeCpp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -fPIC")

if(APPLE)
    # macOS: Enable hardware-specific optimizations
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mtune=native")
elseif(LINUX)
    # Linux: NUMA awareness for distributed systems
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mtune=native")
endif()

# Main library
add_library(ringtree_cpp STATIC
    src/hash.cpp
    src/circle.cpp
    src/ring_tree.cpp
    src/ring_tree_eval.cpp
)

target_include_directories(ringtree_cpp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Tests
enable_testing()

add_executable(ring_tree_test tests/ring_tree_test.cpp)
target_link_libraries(ring_tree_test ringtree_cpp)

add_test(NAME RingTreeTest COMMAND ring_tree_test)

# Benchmark executable (optional)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_executable(ring_tree_bench tests/benchmark.cpp)
    target_link_libraries(ring_tree_bench ringtree_cpp)
endif()

# Installation
install(DIRECTORY include/ DESTINATION include)
install(TARGETS ringtree_cpp DESTINATION lib)
