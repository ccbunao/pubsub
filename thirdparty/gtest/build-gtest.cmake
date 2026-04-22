include_guard(GLOBAL)

# Googletest automatically forces MT instead of MD if we do not set this option.
if(MSVC)
  set(gtest_force_shared_crt ON CACHE BOOL "My option" FORCE)
endif()

set(BUILD_GMOCK OFF CACHE BOOL "My option" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "My option" FORCE)

# Prefer a local checkout in thirdparty/gtest/googletest, but fall back to
# FetchContent so CI can run without submodules.
set(_gtest_root "${CMAKE_CURRENT_LIST_DIR}/googletest")
if(NOT EXISTS "${_gtest_root}/CMakeLists.txt")
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
  )
  FetchContent_MakeAvailable(googletest)
else()
  add_subdirectory("${_gtest_root}" EXCLUDE_FROM_ALL)
endif()

if(NOT TARGET GTest::gtest)
  add_library(GTest::gtest ALIAS gtest)
endif()

if(NOT TARGET GTest::gtest_main)
  add_library(GTest::gtest_main ALIAS gtest_main)
endif()

# Prepend googletest-module/FindGTest.cmake to Module Path
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_LIST_DIR}/Module")
