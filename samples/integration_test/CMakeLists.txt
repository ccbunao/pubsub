cmake_minimum_required(VERSION 3.5.1...4.0)

project(integration_test)

set(CMAKE_CXX_STANDARD 14)

if(NOT TARGET tcp_pubsub::tcp_pubsub)
  set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)
  find_package(tcp_pubsub REQUIRED)
endif()

set(sources
    src/main.cpp
)

add_executable (${PROJECT_NAME}
    ${sources}
)

target_link_libraries (${PROJECT_NAME}
    tcp_pubsub::tcp_pubsub
)
