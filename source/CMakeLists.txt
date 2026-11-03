add_executable(d3d
  actors.c
  demo.c
  game.c
  global.c
  config.c
  gamedef.c
  menus.c
  player.c
  premap.c
  sector.c
  sounds.c
  view.c)

target_include_directories(d3d PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(d3d PRIVATE
  lameduke_compile_options
  buildengine
  audiolib
  ${LAMEDUKE_SDL2MAIN_TARGET})

if(WIN32)
  set_property(TARGET d3d PROPERTY WIN32_EXECUTABLE FALSE)
endif()
