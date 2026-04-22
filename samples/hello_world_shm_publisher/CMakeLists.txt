cmake_minimum_required(VERSION 3.13...4.0)

project(hello_world_shm_publisher LANGUAGES C CXX)

add_executable(hello_world_shm_publisher
  src/main.cpp
)

target_link_libraries(hello_world_shm_publisher
  PRIVATE
    shm_pubsub::shm_pubsub
)

target_compile_features(hello_world_shm_publisher PUBLIC cxx_std_14)

