cmake_minimum_required(VERSION 3.25)
project(splatterhouse LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SH_HOST_SDL "Build with SDL3 present/audio host" ON)
option(SH_BUILD_TESTS "Build unit/smoke tests" ON)

add_library(sh_core STATIC
  src/bus/c117.cpp
  src/bus/virtmap.cpp
  src/cpu/m6809.cpp
  src/cpu/hd63701.cpp
  src/video/video.cpp
  src/audio/audio.cpp
  src/audio/namco_wsg.cpp
  third_party/ymfm/ymfm_opm.cpp
  src/machine/machine.cpp
  src/machine/keycus.cpp
  src/machine/romset.cpp
  src/machine/rom_prepare.cpp
  src/host/null_backends.cpp
  src/host/options_file.cpp
)
target_include_directories(sh_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(sh_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ymfm)
target_compile_definitions(sh_core PUBLIC SH_VERSION=\"0.1.0\")

if(MSVC)
  target_compile_options(sh_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(sh_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(splatterhouse src/main.cpp)
target_link_libraries(splatterhouse PRIVATE sh_core)
if(WIN32)
  target_sources(splatterhouse PRIVATE assets/app.rc)
endif()

if(SH_HOST_SDL)
  include(FetchContent)
  FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.20
    GIT_SHALLOW TRUE
  )
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(SDL3)

  FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.93.0
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(imgui)
  add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
  )
  target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
  )
  target_link_libraries(imgui PUBLIC SDL3::SDL3-static)
  if(MSVC)
    target_compile_options(imgui PRIVATE /W3)
  endif()

  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
  set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
  set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
  set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
  set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
  FetchContent_Declare(
    freetype
    GIT_REPOSITORY https://github.com/freetype/freetype.git
    GIT_TAG VER-2-13-3
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(freetype)
  if(NOT TARGET Freetype::Freetype)
    add_library(Freetype::Freetype ALIAS freetype)
  endif()

  set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
  set(RMLUI_FONT_ENGINE "freetype" CACHE STRING "" FORCE)
  set(RMLUI_LUA_BINDINGS OFF CACHE BOOL "" FORCE)
  set(RMLUI_PRECOMPILED_HEADERS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    RmlUi
    GIT_REPOSITORY https://github.com/mikke89/RmlUi.git
    GIT_TAG 6.1
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(RmlUi)

  target_sources(splatterhouse PRIVATE
    src/host/sdl_backends.cpp
    src/host/play_menu.cpp
    ${rmlui_SOURCE_DIR}/Backends/RmlUi_Platform_SDL.cpp
  )
  target_include_directories(splatterhouse PRIVATE ${rmlui_SOURCE_DIR}/Backends)
  target_compile_definitions(splatterhouse PRIVATE SH_HOST_SDL=1 RMLUI_SDL_VERSION_MAJOR=3)
  target_link_libraries(splatterhouse PRIVATE SDL3::SDL3-static imgui rmlui_core)
  if(WIN32)
    # GUI subsystem: double-click opens the game window with no console flash.
    # Keep main() as the entry point (MSVC would otherwise look for WinMain).
    set_target_properties(splatterhouse PROPERTIES WIN32_EXECUTABLE TRUE)
    if(MSVC)
      target_link_options(splatterhouse PRIVATE "/ENTRY:mainCRTStartup")
    endif()
  endif()
endif()

if(SH_BUILD_TESTS)
  add_executable(sh_smoke tests/smoke_cpu.cpp)
  target_link_libraries(sh_smoke PRIVATE sh_core)
  add_executable(sh_boot_trace tests/boot_trace.cpp)
  target_link_libraries(sh_boot_trace PRIVATE sh_core)
  add_executable(sh_video tests/video.cpp)
  target_link_libraries(sh_video PRIVATE sh_core)
  add_executable(sh_audio tests/audio.cpp)
  target_link_libraries(sh_audio PRIVATE sh_core)
  add_executable(sh_start_sequence tests/start_sequence.cpp)
  target_link_libraries(sh_start_sequence PRIVATE sh_core)
  add_executable(sh_render_state tools/render_video_state.cpp)
  target_link_libraries(sh_render_state PRIVATE sh_core)
  add_executable(sh_trace_damage tools/trace_damage.cpp)
  target_link_libraries(sh_trace_damage PRIVATE sh_core)
  add_executable(sh_trace_audio tools/trace_audio.cpp)
  target_link_libraries(sh_trace_audio PRIVATE sh_core)
  add_executable(sh_capture_audio tools/capture_audio.cpp)
  target_link_libraries(sh_capture_audio PRIVATE sh_core)
  enable_testing()
  add_test(NAME smoke_cpu COMMAND sh_smoke)
  add_test(NAME video COMMAND sh_video)
  add_test(NAME audio COMMAND sh_audio)
  add_test(NAME start_sequence COMMAND sh_start_sequence)
  set_tests_properties(start_sequence PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  set_tests_properties(audio PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

install(TARGETS splatterhouse RUNTIME DESTINATION bin)
