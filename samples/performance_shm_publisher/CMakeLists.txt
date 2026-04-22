cmake_minimum_required(VERSION 3.13...4.0)

project(performance_shm_publisher LANGUAGES C CXX)

add_executable(performance_shm_publisher
  src/main.cpp
)

target_link_libraries(performance_shm_publisher
  PRIVATE
    shm_pubsub::shm_pubsub
)

target_compile_features(performance_shm_publisher PUBLIC cxx_std_14)

