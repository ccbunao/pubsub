cmake_minimum_required(VERSION 3.13...4.0)

project(toolbus_tcp_worker LANGUAGES C CXX)

add_executable(toolbus_tcp_worker
  src/main.cpp
)

target_link_libraries(toolbus_tcp_worker
  PRIVATE
    toolbus::toolbus
    toolbus_tcp::toolbus_tcp
)

target_compile_features(toolbus_tcp_worker PUBLIC cxx_std_14)

