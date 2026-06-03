cmake_minimum_required(VERSION 3.13...4.2.0)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(ZSTD_BUILD_PROGRAMS OFF)
set(ZSTD_BUILD_SHARED OFF)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "Build static library" FORCE)

add_subdirectory(zstd/build/cmake EXCLUDE_FROM_ALL)

if(TARGET libzstd_static)
  dolphin_disable_warnings(libzstd_static)
  add_library(zstd::zstd ALIAS libzstd_static)
else()
  message(FATAL_ERROR "zstd target 'libzstd_static' was not created")
endif()
