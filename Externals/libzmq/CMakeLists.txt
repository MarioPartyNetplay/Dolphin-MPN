set(ENABLE_DRAFTS OFF)
set(BUILD_TESTS OFF)
set(WITH_DOCS OFF)
set(BUILD_SHARED OFF)
set(BUILD_STATIC ON)

# libzmq errors if CMAKE_CURRENT_BINARY_DIR equals its source tree; use a
# dedicated binary dir (needed when the parent uses an in-source layout).
add_subdirectory(libzmq "${CMAKE_CURRENT_BINARY_DIR}/libzmq-build")

dolphin_disable_warnings(objects)
dolphin_disable_warnings(libzmq-static)

if (NOT MSVC)
  target_compile_options(objects PRIVATE "-fexceptions")
  target_compile_options(libzmq-static PRIVATE "-fexceptions")
endif ()

add_library(libzmq::libzmq ALIAS libzmq-static)
