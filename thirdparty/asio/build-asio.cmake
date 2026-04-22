include_guard(GLOBAL)

# asio is a header-only dependency. Prefer a local checkout in
# thirdparty/asio/asio (matching the README), but fall back to FetchContent
# to make the project buildable even without submodules.

set(_asio_root "${CMAKE_CURRENT_LIST_DIR}/asio")
set(_asio_include_dir "${_asio_root}/asio/include")

if(NOT EXISTS "${_asio_include_dir}/asio.hpp")
  include(FetchContent)
  FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-38-0
  )
  FetchContent_GetProperties(asio)
  if(NOT asio_POPULATED)
    FetchContent_Populate(asio)
  endif()

  set(_asio_root "${asio_SOURCE_DIR}")
  set(_asio_include_dir "${_asio_root}/asio/include")
endif()

if(NOT EXISTS "${_asio_include_dir}/asio.hpp")
  message(FATAL_ERROR
    "Could not find asio headers. Expected '${_asio_include_dir}/asio.hpp'. "
    "Either checkout the dependency into 'thirdparty/asio/asio' or allow CMake to fetch it.")
endif()

if(NOT TARGET asio)
  add_library(asio INTERFACE)
  target_include_directories(asio INTERFACE "${_asio_include_dir}")
  target_compile_definitions(asio INTERFACE ASIO_STANDALONE)
endif()

if(NOT TARGET asio::asio)
  add_library(asio::asio ALIAS asio)
endif()

list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_LIST_DIR}/Module")
