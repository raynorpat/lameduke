# SDL2, same idea as duke3d: use a system copy, otherwise build 2.32.10.
set(LAMEDUKE_FETCH_SDL "AUTO" CACHE STRING
  "Download and build SDL2: AUTO (only if not found), ON, OFF")
set_property(CACHE LAMEDUKE_FETCH_SDL PROPERTY STRINGS AUTO ON OFF)

set(_lameduke_fetch_sdl OFF)
if(LAMEDUKE_FETCH_SDL STREQUAL "ON")
  set(_lameduke_fetch_sdl ON)
elseif(NOT LAMEDUKE_FETCH_SDL STREQUAL "OFF")
  find_package(SDL2 CONFIG QUIET)
  if(NOT TARGET SDL2::SDL2 AND NOT TARGET SDL2::SDL2-static)
    set(_lameduke_fetch_sdl ON)
  endif()
endif()

if(_lameduke_fetch_sdl)
  include(FetchContent)
  FetchContent_Declare(SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.32.10
    GIT_SHALLOW TRUE)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(SDL2)
endif()

if(TARGET SDL2-static)
  set(LAMEDUKE_SDL2_TARGET SDL2-static)
elseif(TARGET SDL2::SDL2-static)
  set(LAMEDUKE_SDL2_TARGET SDL2::SDL2-static)
elseif(TARGET SDL2::SDL2)
  set(LAMEDUKE_SDL2_TARGET SDL2::SDL2)
else()
  find_package(SDL2 CONFIG REQUIRED)
  set(LAMEDUKE_SDL2_TARGET SDL2::SDL2)
endif()

if(TARGET SDL2main)
  set(LAMEDUKE_SDL2MAIN_TARGET SDL2main)
elseif(TARGET SDL2::SDL2main)
  set(LAMEDUKE_SDL2MAIN_TARGET SDL2::SDL2main)
else()
  add_library(lameduke_sdl2main_stub INTERFACE)
  set(LAMEDUKE_SDL2MAIN_TARGET lameduke_sdl2main_stub)
endif()
