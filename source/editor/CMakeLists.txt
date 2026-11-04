# The BUILD map editor: Ken Silverman's build.c from raynorpat/duke3d
# (see source/build/VENDORED.md), on this repo's 1994 engine. The target
# is not called "build", so it can't clash with the build/ directory.

add_executable(buildeditor build.c lstub.c edglue.c)
set_target_properties(buildeditor PROPERTIES OUTPUT_NAME build)
target_link_libraries(buildeditor PRIVATE lameduke_compile_options buildengine ${LAMEDUKE_SDL2MAIN_TARGET})
if(WIN32)
  set_property(TARGET buildeditor PROPERTY WIN32_EXECUTABLE FALSE)
endif()
