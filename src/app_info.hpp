#pragma once

#include "app_version.hpp"

// Single source of truth for product/identity strings shared across modules.
// Registry-based identity is kept separate from display text so that a future
// rename only touches this file.

namespace vmwake {
namespace appinfo {

// Display name shown in the window title, tray tooltip and balloon.
inline constexpr const wchar_t* kDisplayName = L"Voicemeeter Wake";

// Win32 window class registered for the main window and used for
// single-instance lookup.
inline constexpr const wchar_t* kWindowClass = L"VoicemeeterWakeWindow";

// Named mutex (Local\ namespace) guarding single-instance behaviour.
inline constexpr const wchar_t* kMutexName =
    L"Local\\VoicemeeterWake.SingleInstance.0A2F";

// HKCU registry key holding the application settings.
inline constexpr const wchar_t* kRegistryPath = L"Software\\VoicemeeterWake";

// Value name under HKCU\...\Run used for run-at-startup.
inline constexpr const wchar_t* kRunValue = L"VoicemeeterWake";

inline constexpr const wchar_t* kProjectUrl =
    L"https://github.com/du33169/voicemeeter-wake";
inline constexpr const wchar_t* kReadmeUrl =
    L"https://github.com/du33169/voicemeeter-wake#readme";
inline constexpr const wchar_t* kVoicemeeterUrl =
    L"https://www.voicemeeter.com";

} // namespace appinfo
} // namespace vmwake
