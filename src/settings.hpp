#pragma once

#include <string>

namespace vmwake {

struct AppSettings {
    bool enabled = true;
    float play_threshold_db = -50.0f;
    float silence_threshold_db = -55.0f;
    int confirm_samples = 3;
    int arm_after_ms = 600000; // 600 s
    int cooldown_ms = 5000;    // 5 s
    bool start_minimized = false;
    int restart_count = 0;
    std::wstring last_restart; // empty if never
};

namespace settings {

const wchar_t* registry_path();

// Returns defaults (and writes nothing) if the key is absent.
AppSettings load();

void save(const AppSettings& s);

// Run-key autostart ("HKCU\...\Run").  false disables.
bool get_autostart();
void set_autostart(bool enabled);

} // namespace settings

} // namespace vmwake
