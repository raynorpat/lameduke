# The BUILD map editor: Ken Silverman's build.c from raynorpat/duke3d
# (see source/build/VENDORED.md), on this repo's 1994 engine. The target
# is not called "build", so it can't clash with the build/ directory.

add_executable(buildeditor build.c lstub.c edglue.c buildcfg.c)
set_target_properties(buildeditor PROPERTIES OUTPUT_NAME build)
target_link_libraries(buildeditor PRIVATE lameduke_compile_options buildengine videocfg ${LAMEDUKE_SDL2MAIN_TARGET})
if(WIN32)
  set_property(TARGET buildeditor PROPERTY WIN32_EXECUTABLE FALSE)
  target_sources(buildeditor PRIVATE build.rc)
  set_source_files_properties(build.rc PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../rsrc/build.ico")
endif()
