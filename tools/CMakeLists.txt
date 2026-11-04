# Map tools: the checker the editor checks run, and later the v4 converter.

add_executable(mapcheck mapcheck.c)
target_link_libraries(mapcheck PRIVATE lameduke_compile_options buildengine)

add_executable(convmap4 convmap4.c)
target_link_libraries(convmap4 PRIVATE lameduke_compile_options buildengine)
