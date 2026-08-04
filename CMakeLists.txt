cmake_minimum_required(VERSION 3.30)

# Single source of truth for the project version: VERSION at the repo root.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" VMWAKE_VERSION_RAW)
string(STRIP "${VMWAKE_VERSION_RAW}" VMWAKE_VERSION)
string(REPLACE "." "," VMWAKE_VERSION_COMMA "${VMWAKE_VERSION}")

project(voicemeeter-wake VERSION "${VMWAKE_VERSION}" LANGUAGES C CXX)

set(VMWAKE_EXECUTABLE ${PROJECT_NAME})
set(VMWAKE_PRODUCT_NAME "Voicemeeter Wake")
set(VMWAKE_DESCRIPTION "Restart Voicemeeter audio engine when playback resumes after long silence (headphone wake-up)")
set(VMWAKE_COMPANY "Voicemeeter Wake")

configure_file(resources/app.rc.in "${CMAKE_CURRENT_BINARY_DIR}/app.rc" @ONLY)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Core Windows GUI target -----------------------------------------------------
add_executable(${VMWAKE_EXECUTABLE} WIN32
  ${CMAKE_CURRENT_BINARY_DIR}/app.rc
  src/main.cpp
  src/application.cpp
  src/ui/main_window.cpp
  src/ui/main_window_ui.cpp
  src/ui/main_window_monitor.cpp
  src/ui/main_window_tray.cpp
  src/ui/win_util.cpp
  src/audio_monitor.cpp
  src/voicemeeter_remote.cpp
  src/settings.cpp
  src/logger.cpp
)

target_include_directories(${VMWAKE_EXECUTABLE} PRIVATE src src/ui)

target_compile_definitions(${VMWAKE_EXECUTABLE} PRIVATE
  UNICODE
  _UNICODE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
  _WIN32_WINNT=0x0A00
)

if(MINGW)
  set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I ${CMAKE_CURRENT_SOURCE_DIR}/resources")

  target_compile_options(${VMWAKE_EXECUTABLE} PRIVATE
    -Wall -Wextra
    -municode
  )
  # Resolve common CRT symbols from the system UCRT before conda-forge's
  # fallback private import library is considered.
  target_link_options(${VMWAKE_EXECUTABLE} PRIVATE
    -municode
    -mwindows
    -static
    -Wl,--undefined=memchr
    -Wl,--undefined=memcmp
    -Wl,--undefined=memcpy
    -Wl,--undefined=memmove
    -Wl,--undefined=strchr
  )
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_options(${VMWAKE_EXECUTABLE} PRIVATE -s)
  endif()

endif()

target_link_libraries(${VMWAKE_EXECUTABLE} PRIVATE
  user32
  shell32
  advapi32
  comctl32
  dwmapi
  gdi32
  ucrtbase
)

# Unit tests ------------------------------------------------------------------
enable_testing()

add_executable(audio_monitor_tests
  tests/audio_monitor_tests.cpp
  src/audio_monitor.cpp
)

target_include_directories(audio_monitor_tests PRIVATE src)
target_compile_definitions(audio_monitor_tests PRIVATE
  UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00
)
set_target_properties(audio_monitor_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

if(MINGW)
  target_compile_options(audio_monitor_tests PRIVATE -Wall -Wextra)
  target_link_options(audio_monitor_tests PRIVATE -static)
endif()

add_test(NAME audio_monitor COMMAND audio_monitor_tests)
