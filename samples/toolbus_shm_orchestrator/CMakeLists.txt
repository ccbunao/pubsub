cmake_minimum_required(VERSION 3.13...4.0)

project(toolbus_shm_orchestrator LANGUAGES C CXX)

add_executable(toolbus_shm_orchestrator
  src/main.cpp
)

target_link_libraries(toolbus_shm_orchestrator
  PRIVATE
    shm_pubsub::shm_pubsub
    toolbus::toolbus
)

target_compile_features(toolbus_shm_orchestrator PUBLIC cxx_std_14)

