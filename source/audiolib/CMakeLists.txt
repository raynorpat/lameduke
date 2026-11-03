# Jim Dose AudioLib as linked in D3D.EXE. The DOS card drivers are still
# here because the binary calls them. Digital output goes through dsl.c
# (SDL), the same split duke3d uses. OPL playback is oplemu.c.

add_library(audiolib STATIC
  fx_man.c
  music.c
  multivoc.c
  mv_mix.c
  midi.c
  al_midi.c
  ll_man.c
  pitch.c
  user.c
  task_man.c
  dsl.c
  oplemu.c
  blaster.c
  pas16.c
  adlibfx.c
  gus.c
  guswave.c
  gusmidi.c
  sndsrc.c
  pcfx.c
  mpu401.c
  awe32.c)

target_include_directories(audiolib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(audiolib PUBLIC lameduke_compile_options ${LAMEDUKE_SDL2_TARGET})
if(NOT WIN32 AND NOT APPLE)
  target_link_libraries(audiolib PUBLIC m)
endif()
