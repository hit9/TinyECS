cmake_minimum_required(VERSION 3.10)
project(tinyecs_example CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories("..")

# Targets
add_executable(tinyecs_example main.cc ../tinyecs.cc)
