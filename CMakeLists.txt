cmake_minimum_required(VERSION 3.24)

project(lameduke LANGUAGES C)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

# Software renderer from D3D.EXE. OpenGL is only the presentation path,
# following duke3d's SDL/GL platform layer. Polymost is not in this binary.
option(LAMEDUKE_OPENGL "Present the framebuffer through OpenGL" ON)
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
