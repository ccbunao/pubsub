cmake_minimum_required(VERSION 3.13...4.0)

project(toolbus LANGUAGES C CXX)

include(FetchContent)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

add_library(toolbus
  src/id.cpp
  src/message.cpp
  src/dedup_cache.cpp
)

add_library(toolbus::toolbus ALIAS toolbus)

target_compile_features(toolbus PUBLIC cxx_std_14)

target_link_libraries(toolbus
  PUBLIC
    nlohmann_json::nlohmann_json
)

target_include_directories(toolbus
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    src
)

