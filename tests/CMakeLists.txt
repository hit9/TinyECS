cmake_minimum_required(VERSION 3.10)

project(TinyECS_Tests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

enable_testing()

include_directories(".." ".")

find_package(Catch2 CONFIG REQUIRED)

# Targets
file(GLOB TEST_SOURCES *Test.cpp ../TinyECS.cpp Shares.cpp)
add_executable(TinyECS_Tests ${TEST_SOURCES})

target_link_libraries(TinyECS_Tests PRIVATE Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(TinyECS_Tests)
