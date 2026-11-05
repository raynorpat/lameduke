# Build engine linked inside D3D.EXE. DOS video, the PIT and the mouse
# IRQ are replaced by the sdl_* files. There is no GRP and no separate
# BUILD.EXE in this tree.

add_library(buildengine STATIC
  board.c
  cache.c
  draw.c
  display.c
  engine.c
  math.c
  network.c
  tiles.c
  sdl_main.c
  sdl_input.c
  sdl_window.c
  sdl_sys.c
  sdl_audio.c)

if(WIN32)
  target_sources(buildengine PRIVATE plat_win32.c)
else()
  target_sources(buildengine PRIVATE plat_unix.c)
endif()

target_include_directories(buildengine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(buildengine PUBLIC lameduke_compile_options ${LAMEDUKE_SDL2_TARGET})

if(LAMEDUKE_OPENGL)
  find_package(OpenGL REQUIRED)
  target_compile_definitions(buildengine PUBLIC USE_OPENGL=1)
  if(APPLE)
    target_compile_definitions(buildengine PUBLIC GL_SILENCE_DEPRECATION)
  endif()
  target_link_libraries(buildengine PUBLIC OpenGL::GL)
endif()

if(WIN32)
  target_link_libraries(buildengine PUBLIC winmm)
elseif(NOT APPLE)
  target_link_libraries(buildengine PUBLIC m)
endif()
