if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.13...4.0)
  project(shm_pubsub LANGUAGES C CXX)
endif()

include(GenerateExportHeader)

find_package(Threads REQUIRED)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

set(SHM_PUBSUB_LIBRARY_TYPE "" CACHE STRING "Controls the library type of shm_pubsub by injecting the string into the add_library call (STATIC/SHARED/OBJECT)")

set(includes
  include/shm_pubsub/callback_data.h
  include/shm_pubsub/shm_pubsub_logger.h
  include/shm_pubsub/shm/publisher.h
  include/shm_pubsub/shm/subscriber.h
)

set(sources
  src/publisher.cpp
  src/publisher_impl.cpp
  src/publisher_impl.h
  src/subscriber.cpp
  src/subscriber_impl.cpp
  src/subscriber_impl.h

  src/shm/atomic_u64.h
  src/shm/shm_layout.h
  src/shm/shm_name.h
  src/shm/shm_region.h
  src/shm/shm_region_posix.cpp
  src/shm/shm_region_win32.cpp
)

add_library(shm_pubsub ${SHM_PUBSUB_LIBRARY_TYPE}
  ${includes}
  ${sources}
)

generate_export_header(shm_pubsub
  EXPORT_FILE_NAME ${PROJECT_BINARY_DIR}/include/shm_pubsub/shm_pubsub_export.h
  BASE_NAME SHM_PUBSUB
)

add_library(shm_pubsub::shm_pubsub ALIAS shm_pubsub)

target_link_libraries(shm_pubsub
  PRIVATE
    Threads::Threads
)

target_compile_features(shm_pubsub PUBLIC cxx_std_14)

target_compile_definitions(shm_pubsub
  PRIVATE
    _WIN32_WINNT=0x0601
)

target_compile_options(shm_pubsub PRIVATE
  $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
    -Wall -Wextra>
  $<$<CXX_COMPILER_ID:MSVC>:
    /W4>
)

target_include_directories(shm_pubsub
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    src
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES
  ${includes}
  ${sources}
)
