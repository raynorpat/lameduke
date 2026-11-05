add_library(videocfg STATIC videocfg.c)
target_include_directories(videocfg PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(videocfg PUBLIC buildengine)

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
  videocfg
  audiolib
  ${LAMEDUKE_SDL2MAIN_TARGET})

if(WIN32)
  set_property(TARGET d3d PROPERTY WIN32_EXECUTABLE FALSE)
  target_sources(d3d PRIVATE d3d.rc)
  set_source_files_properties(d3d.rc PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/rsrc/duke3d.ico")
endif()
