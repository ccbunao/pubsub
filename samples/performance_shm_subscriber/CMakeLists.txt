cmake_minimum_required(VERSION 3.13...4.0)

project(performance_shm_subscriber LANGUAGES C CXX)

add_executable(performance_shm_subscriber
  src/main.cpp
)

target_link_libraries(performance_shm_subscriber
  PRIVATE
    shm_pubsub::shm_pubsub
)

target_compile_features(performance_shm_subscriber PUBLIC cxx_std_14)

