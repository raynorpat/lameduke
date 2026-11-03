include(CheckCCompilerFlag)

add_library(lameduke_compile_options INTERFACE)

target_compile_definitions(lameduke_compile_options INTERFACE USE_SDL=1)

if(WIN32)
  target_compile_definitions(lameduke_compile_options INTERFACE PLATFORM_WIN32=1)
elseif(APPLE)
  target_compile_definitions(lameduke_compile_options INTERFACE
    PLATFORM_UNIX=1 PLATFORM_MACOSX=1)
else()
  target_compile_definitions(lameduke_compile_options INTERFACE PLATFORM_UNIX=1)
endif()

if(MSVC)
  target_compile_definitions(lameduke_compile_options INTERFACE
    _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
  # The original Watcom build treats plain char as unsigned.
  target_compile_options(lameduke_compile_options INTERFACE /J /W3)
else()
  target_compile_options(lameduke_compile_options INTERFACE
    -funsigned-char -fno-strict-aliasing -Wall)
  foreach(_flag implicit-function-declaration implicit-int int-conversion incompatible-pointer-types)
    string(MAKE_C_IDENTIFIER "LAMEDUKE_WNO_${_flag}" _var)
    check_c_compiler_flag("-Wno-error=${_flag}" ${_var})
    if(${_var})
      target_compile_options(lameduke_compile_options INTERFACE
        "$<$<COMPILE_LANGUAGE:C>:-Wno-error=${_flag}>")
    endif()
  endforeach()
endif()

target_compile_features(lameduke_compile_options INTERFACE c_std_99)
