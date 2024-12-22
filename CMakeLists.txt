cmake_minimum_required(VERSION 3.10)
project(DistributedDataStore CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Toggle GPU code if you have CUDA
option(USE_CUDA "Enable CUDA for GPU-accelerated analytics" OFF)

if(USE_CUDA)
    add_definitions(-DUSE_CUDA)
    find_package(CUDA REQUIRED)
    include_directories(${CUDA_INCLUDE_DIRS})
endif()

# Toggle building tests
option(BUILD_TESTS "Build Google Tests" ON)

# Include dirs
include_directories(${PROJECT_SOURCE_DIR}/include)

# Collect source files
set(SOURCES
    src/concurrency.cpp
    src/datastore.cpp
    src/distributed_node.cpp
)

# Build as a static library
add_library(datastore_lib STATIC ${SOURCES})

# Create the main demo (only if not building under test)
add_executable(dist_demo src/main.cpp)
target_link_libraries(dist_demo PRIVATE datastore_lib)

# Tests
if(BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)

    add_executable(dist_tests
        tests/test_main.cpp
        tests/test_suite.cpp
    )

    # Define UNIT_TEST so #ifdef UNIT_TEST blocks are compiled
    target_compile_definitions(dist_tests PRIVATE UNIT_TEST)

    target_link_libraries(dist_tests
        PRIVATE datastore_lib
        PRIVATE GTest::GTest
        PRIVATE GTest::Main
        pthread
    )

    add_test(NAME DistributedDataStoreTests COMMAND dist_tests)
endif()
