cmake_minimum_required(VERSION 3.24)

project(lameduke LANGUAGES C)

if(WIN32)
  enable_language(RC)
endif()

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

# Classic software rendering is reconstructed from D3D.EXE. Polymost and
# its OpenGL backend come from duke3d and can be disabled independently.
option(LAMEDUKE_OPENGL "Enable the Polymost OpenGL renderer" ON)
option(LAMEDUKE_POLYMOST "Build Polymost's renderers" ON)
set(LAMEDUKE_FETCH_SDL "AUTO" CACHE STRING "See cmake/Dependencies.cmake")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Dependencies)
include(CompileOptions)

add_subdirectory(source/build)
add_subdirectory(source/audiolib)
add_subdirectory(source)
add_subdirectory(source/editor)
add_subdirectory(tools)
