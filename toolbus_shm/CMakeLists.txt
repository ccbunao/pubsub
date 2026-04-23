cmake_minimum_required(VERSION 3.13...4.0)

project(toolbus_shm LANGUAGES C CXX)

add_library(toolbus_shm
  src/publisher.cpp
  src/subscription.cpp
)

add_library(toolbus_shm::toolbus_shm ALIAS toolbus_shm)

target_compile_features(toolbus_shm PUBLIC cxx_std_14)

target_link_libraries(toolbus_shm
  PUBLIC
    toolbus::toolbus
    shm_pubsub::shm_pubsub
)

target_include_directories(toolbus_shm
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    src
)

