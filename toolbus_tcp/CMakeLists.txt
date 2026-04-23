cmake_minimum_required(VERSION 3.13...4.0)

project(toolbus_tcp LANGUAGES C CXX)

add_library(toolbus_tcp
  src/mux.cpp
  src/publisher.cpp
  src/subscriber.cpp
)

add_library(toolbus_tcp::toolbus_tcp ALIAS toolbus_tcp)

target_compile_features(toolbus_tcp PUBLIC cxx_std_14)

target_link_libraries(toolbus_tcp
  PUBLIC
    toolbus::toolbus
    tcp_pubsub::tcp_pubsub
)

target_include_directories(toolbus_tcp
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    src
)

