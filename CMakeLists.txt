cmake_minimum_required(VERSION 3.21)

# Release builds pass the version computed from git tags: -DBK_VERSION=1.2.3
set(BK_VERSION "0.0.0" CACHE STRING "Version embedded in the binary (major.minor.patch)")
if(NOT BK_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
  message(FATAL_ERROR "BK_VERSION must look like 1.2.3, got '${BK_VERSION}'")
endif()
set(BK_VERSION_MAJOR ${CMAKE_MATCH_1})
set(BK_VERSION_MINOR ${CMAKE_MATCH_2})
set(BK_VERSION_PATCH ${CMAKE_MATCH_3})

project(BrightKeys
  VERSION ${BK_VERSION}
  DESCRIPTION "Control external monitor brightness (DDC/CI) with keyboard hotkeys"
  HOMEPAGE_URL "https://github.com/tejashwikalptaru/brightkeys"
  LANGUAGES C RC)

if(NOT MSVC)
  message(FATAL_ERROR "BrightKeys is built with MSVC (Visual Studio 2019 or newer).")
endif()

# Static CRT: the exe runs without the Visual C++ Redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# Optimize release builds for size; this is a background utility.
string(REPLACE "/O2" "/O1" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")

add_executable(BrightKeys WIN32
  src/about.c
  src/brightness.c
  src/ddc.c
  src/gfx.c
  src/input.c
  src/learn.c
  src/main.c
  src/osd.c
  src/popup.c
  src/settings.c
  src/tray.c
  res/brightkeys.rc)

set_target_properties(BrightKeys PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED ON)
target_include_directories(BrightKeys PRIVATE src)
target_compile_definitions(BrightKeys PRIVATE
  UNICODE _UNICODE
  BK_VERSION_MAJOR=${BK_VERSION_MAJOR}
  BK_VERSION_MINOR=${BK_VERSION_MINOR}
  BK_VERSION_PATCH=${BK_VERSION_PATCH})
target_compile_options(BrightKeys PRIVATE $<$<COMPILE_LANGUAGE:C>:/W4 /WX /utf-8 /GS>)
target_link_options(BrightKeys PRIVATE
  /MANIFEST:NO   # the manifest is embedded by res/brightkeys.rc
  $<$<CONFIG:Release>:/OPT:REF /OPT:ICF>)
target_link_libraries(BrightKeys PRIVATE user32 gdi32 shell32 advapi32 dxva2 hid msimg32 dwmapi shcore)
