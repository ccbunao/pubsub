include_guard(GLOBAL)

# recycle is a header-only dependency. The upstream project is integrated with
# the steinwurf toolchain and expects STEINWURF_RESOLVE to be set, which we
# don't want to require for this repo. Therefore we provide our own minimal
# interface target here.

set(_recycle_root "${CMAKE_CURRENT_LIST_DIR}/recycle")
set(_recycle_include_dir "${_recycle_root}/src")

if(NOT EXISTS "${_recycle_include_dir}/recycle/shared_pool.hpp")
  include(FetchContent)
  FetchContent_Declare(
    recycle
    GIT_REPOSITORY https://github.com/steinwurf/recycle.git
    GIT_TAG 8.0.0
  )
  FetchContent_GetProperties(recycle)
  if(NOT recycle_POPULATED)
    FetchContent_Populate(recycle)
  endif()

  set(_recycle_root "${recycle_SOURCE_DIR}")
  set(_recycle_include_dir "${_recycle_root}/src")
endif()

if(NOT EXISTS "${_recycle_include_dir}/recycle/shared_pool.hpp")
  message(FATAL_ERROR
    "Could not find recycle headers. Expected '${_recycle_include_dir}/recycle/shared_pool.hpp'. "
    "Either checkout the dependency into 'thirdparty/recycle/recycle' or allow CMake to fetch it.")
endif()

if(NOT TARGET recycle)
  add_library(recycle INTERFACE)
  target_compile_features(recycle INTERFACE cxx_std_14)
  target_include_directories(recycle INTERFACE "${_recycle_include_dir}")
endif()

if(NOT TARGET steinwurf::recycle)
  add_library(steinwurf::recycle ALIAS recycle)
endif()

# Prepend recycle-module/Findrecycle.cmake to the module path
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_LIST_DIR}/Module")
