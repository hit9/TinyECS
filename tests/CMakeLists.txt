cmake_minimum_required(VERSION 3.10)

project(tinyecs_tests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

enable_testing()

include_directories(".." ".")

find_package(Catch2 CONFIG REQUIRED)

# Targets
file(GLOB TEST_SOURCES *_test.cc ../tinyecs.cc shares.cc)
file(GLOB BENCHMARK_SOURCES *_benchmark.cc ../tinyecs.cc)
add_executable(tinyecs_tests ${TEST_SOURCES})
# add_executable(tinyecs_benchmark ${BENCHMARK_SOURCES})

target_link_libraries(tinyecs_tests PRIVATE Catch2::Catch2WithMain)
# target_link_libraries(tinyecs_benchmark PRIVATE Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(tinyecs_tests)
