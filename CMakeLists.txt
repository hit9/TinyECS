cmake_minimum_required(VERSION 3.10)

project(tinyecs)

set(CMAKE_CXX_STANDARD 20)

add_library(tinyecs SHARED tinyecs.cc)
set_target_properties(tinyecs PROPERTIES PUBLIC_HEADER "tinyecs.h")

install(
  TARGETS tinyecs
  LIBRARY DESTINATION ${CMAK_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
