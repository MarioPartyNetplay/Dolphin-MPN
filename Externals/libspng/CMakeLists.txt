option(SPNG_SHARED "Build shared lib" OFF)
option(SPNG_STATIC "Build static lib" ON)
option(BUILD_EXAMPLES "Build examples" OFF)

set(_PREV_CMAKE_SKIP_INSTALL_RULES "${CMAKE_SKIP_INSTALL_RULES}")
set(CMAKE_SKIP_INSTALL_RULES TRUE)

add_subdirectory(libspng)

set(CMAKE_SKIP_INSTALL_RULES "${_PREV_CMAKE_SKIP_INSTALL_RULES}")

add_library(spng::spng ALIAS spng_static)

# Fix include directories for spng_static target
if(TARGET spng_static)
  target_include_directories(spng_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/libspng/spng)
endif()
