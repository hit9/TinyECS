cmake_minimum_required(VERSION 3.10)
project(TinyECS_Example CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Targets
include_directories(../Source)
add_executable(TinyECS_Example main.cpp ../Source/TinyECS.cpp)
