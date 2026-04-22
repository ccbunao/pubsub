cmake_minimum_required(VERSION 3.13...4.0)

project(hello_world_shm_subscriber LANGUAGES C CXX)

add_executable(hello_world_shm_subscriber
  src/main.cpp
)

target_link_libraries(hello_world_shm_subscriber
  PRIVATE
    shm_pubsub::shm_pubsub
)

target_compile_features(hello_world_shm_subscriber PUBLIC cxx_std_14)

