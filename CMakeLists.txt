cmake_minimum_required(VERSION 3.30)

project(voicemeeter-engine-wake VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(APP_EXECUTABLE voicemeeter-engine-wake)

# Core Windows GUI target -----------------------------------------------------
add_executable(${APP_EXECUTABLE} WIN32
  resources/app.rc
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

target_include_directories(${APP_EXECUTABLE} PRIVATE src src/ui)

target_compile_definitions(${APP_EXECUTABLE} PRIVATE
  UNICODE
  _UNICODE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
  _WIN32_WINNT=0x0A00
)

if(MINGW)
  target_compile_options(${APP_EXECUTABLE} PRIVATE
    -Wall -Wextra
    -municode
  )
  target_link_options(${APP_EXECUTABLE} PRIVATE
    -municode
    -mwindows
    -static
    -s
  )
endif()

target_link_libraries(${APP_EXECUTABLE} PRIVATE
  user32
  shell32
  advapi32
  comctl32
  dwmapi
  gdi32
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
  target_link_options(audio_monitor_tests PRIVATE -static)
endif()

add_test(NAME audio_monitor COMMAND audio_monitor_tests)
