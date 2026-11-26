cmake_minimum_required(VERSION 3.16)
project(FruitSlicer CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(AUTOTEST "Build the self-playing test bot" OFF)
set(TESTDEFS "" CACHE STRING "Extra defines for test builds")
# Developer tools, such as coins on demand in Options. Never on for release builds.
option(DEV_TOOLS "Build the developer tools into Options" OFF)
# The Lab build: the bot runs a quick check of every feature and writes lab.txt (source/lab.inc). Developer only.
option(LAB "Build the Lab: a quick self-check of every feature" OFF)
if(LAB)
  set(AUTOTEST ON)
  set(DEV_TOOLS ON)
  set(TESTDEFS "${TESTDEFS};LAB")
endif()

# SDL3 is built in, so players never need to install anything.
include(FetchContent)
if(ANDROID)
  # Android loads SDL's Java glue from its own library.
  set(SDL_SHARED ON CACHE BOOL "" FORCE)
  set(SDL_STATIC OFF CACHE BOOL "" FORCE)
else()
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
endif()
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3 URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.2/SDL3-3.4.2.tar.gz)
FetchContent_MakeAvailable(SDL3)

file(GLOB GAME_SOURCES source/*.cpp port/*.cpp)
if(ANDROID)
  add_library(main SHARED ${GAME_SOURCES})
  set(GAME_TARGET main)
else()
  if(WIN32)
    enable_language(RC)
    list(APPEND GAME_SOURCES port/windows.rc)
  endif()
  add_executable(FruitSlicer WIN32 ${GAME_SOURCES})
  set(GAME_TARGET FruitSlicer)
endif()
target_include_directories(${GAME_TARGET} PRIVATE source port port/generated)
if(ANDROID)
  target_link_libraries(${GAME_TARGET} PRIVATE SDL3::SDL3)
else()
  target_link_libraries(${GAME_TARGET} PRIVATE SDL3::SDL3-static)
endif()
if(MINGW)
  target_link_options(${GAME_TARGET} PRIVATE -static -static-libgcc -static-libstdc++)
  target_link_libraries(${GAME_TARGET} PRIVATE psapi winhttp)
endif()
if(AUTOTEST)
  target_compile_definitions(${GAME_TARGET} PRIVATE AUTOTEST ${TESTDEFS})
endif()
if(UNIX AND NOT ANDROID)
  target_link_libraries(${GAME_TARGET} PRIVATE ${CMAKE_DL_LIBS})  # libcurl is loaded at run time for updates
endif()
if(DEV_TOOLS)
  target_compile_definitions(${GAME_TARGET} PRIVATE DEV_TOOLS)
endif()
if(EMSCRIPTEN)
  # The browser version: FruitSlicer.html, .js, .wasm and .data, the game's files packed in the .data.
  # ASYNCIFY lets the game's own loops hand each frame back to the browser, so the shared code stays as it is.
  # The 4K art is left out (no Ultra in a browser), and the save lives in the browser's IndexedDB (web/pre.js).
  set_target_properties(${GAME_TARGET} PROPERTIES SUFFIX ".html")
  target_link_options(${GAME_TARGET} PRIVATE
    -sASYNCIFY -sASYNCIFY_STACK_SIZE=262144 -sSTACK_SIZE=1048576
    -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728
    -sEXPORTED_FUNCTIONS=_main,_malloc,_free -sEXPORTED_RUNTIME_METHODS=FS,UTF8ToString,stringToUTF8 -lidbfs.js
    "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/romfs@/romfs"
    "SHELL:--exclude-file ${CMAKE_SOURCE_DIR}/romfs/gfx4k"
    "SHELL:--exclude-file ${CMAKE_SOURCE_DIR}/romfs/tex4k"
    "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/web/pre.js"
    "SHELL:--shell-file ${CMAKE_SOURCE_DIR}/web/shell.html")
  set_property(TARGET ${GAME_TARGET} APPEND PROPERTY LINK_DEPENDS ${CMAKE_SOURCE_DIR}/web/pre.js ${CMAKE_SOURCE_DIR}/web/shell.html)
elseif(NOT ANDROID)
  add_custom_command(TARGET ${GAME_TARGET} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/romfs $<TARGET_FILE_DIR:${GAME_TARGET}>/romfs)
endif()

if(DEV_TOOLS AND NOT ANDROID AND NOT EMSCRIPTEN AND NOT LAB)
  add_executable(OrchardBlastBossTest WIN32 ${GAME_SOURCES})
  target_include_directories(OrchardBlastBossTest PRIVATE source port port/generated)
  target_link_libraries(OrchardBlastBossTest PRIVATE SDL3::SDL3-static)
  if(MINGW)
    target_link_options(OrchardBlastBossTest PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_libraries(OrchardBlastBossTest PRIVATE psapi winhttp)
  endif()
  if(UNIX)
    target_link_libraries(OrchardBlastBossTest PRIVATE ${CMAKE_DL_LIBS})
  endif()
  target_compile_definitions(OrchardBlastBossTest PRIVATE DEV_TOOLS BOSS_TEST_APP)
endif()
