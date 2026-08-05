#include "settings.hpp"

#include <windows.h>
#include <algorithm>
#include <limits>
#include <vector>

#include "app_info.hpp"

namespace vmwake {
namespace settings {

namespace {

constexpr wchar_t kRunPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

std::wstring current_exe_path() {
    std::vector<wchar_t> buf(MAX_PATH);
    for (int attempt = 0; attempt < 2; ++attempt) {
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(),
                                           static_cast<DWORD>(buf.size()));
        if (n == 0) return {};
        if (n + 1 < buf.size()) {
            buf[n] = 0;
            return std::wstring(buf.data());
        }
        buf.resize(buf.size() * 2);
    }
    return {};
}

// Reads every known value from an open settings key.
void read_from_key(HKEY key, AppSettings& s) {
    auto read_dword = [&](const wchar_t* name, DWORD* out) {
        DWORD size = sizeof(DWORD);
        DWORD type = 0;
        return RegQueryValueExW(key, name, nullptr, &type,
                                reinterpret_cast<LPBYTE>(out), &size) ==
                   ERROR_SUCCESS &&
               type == REG_DWORD;
    };
    auto read_str = [&](const wchar_t* name, std::wstring* out) {
        DWORD size = 0;
        DWORD type = 0;
        if (RegQueryValueExW(key, name, nullptr, &type, nullptr, &size) !=
                ERROR_SUCCESS ||
            type != REG_SZ || size == 0) {
            return false;
        }
        std::vector<wchar_t> buf(size / sizeof(wchar_t) + 1, 0);
        if (RegQueryValueExW(key, name, nullptr, &type,
                             reinterpret_cast<LPBYTE>(buf.data()), &size) !=
            ERROR_SUCCESS) {
            return false;
        }
        *out = buf.data();
        return true;
    };

    DWORD v = 0;
    if (read_dword(L"Enabled", &v)) s.enabled = v != 0;
    if (read_dword(L"PlayThresholdDb", &v)) s.play_threshold_db = static_cast<float>(static_cast<long>(v)) / 100.0f;
    if (read_dword(L"SilenceThresholdDb", &v)) s.silence_threshold_db = static_cast<float>(static_cast<long>(v)) / 100.0f;
    if (read_dword(L"ConfirmSamples", &v))
        s.confirm_samples = static_cast<int>(std::min<DWORD>(v, 100));
    if (read_dword(L"ArmAfterMs", &v))
        s.arm_after_ms = static_cast<int>(std::min<DWORD>(v, 120 * 60000));
    if (read_dword(L"CooldownMs", &v))
        s.cooldown_ms = static_cast<int>(std::min<DWORD>(v, 60000));
    if (read_dword(L"OutputBusMask", &v)) s.output_bus_mask = v & 0x1f;
    if (read_dword(L"StartMinimized", &v)) s.start_minimized = v != 0;
    if (read_dword(L"RestartCount", &v))
        s.restart_count = static_cast<int>(std::min<DWORD>(
            v, static_cast<DWORD>(std::numeric_limits<int>::max())));
    read_str(L"LastRestart", &s.last_restart);
}

} // namespace

AppSettings load() {
    AppSettings s;

    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, appinfo::kRegistryPath, 0, KEY_READ,
                      &key) == ERROR_SUCCESS) {
        read_from_key(key, s);
        RegCloseKey(key);
    }

    s.play_threshold_db = std::clamp(s.play_threshold_db, -55.0f, 0.0f);
    s.silence_threshold_db = std::clamp(s.silence_threshold_db, -60.0f, 0.0f);
    if (s.silence_threshold_db >= s.play_threshold_db) {
        s.silence_threshold_db = s.play_threshold_db - 5.0f;
    }
    s.confirm_samples = std::clamp(s.confirm_samples, 1, 100);
    s.arm_after_ms = std::clamp(s.arm_after_ms, 60000, 120 * 60000);
    s.cooldown_ms = std::clamp(s.cooldown_ms, 1000, 60000);
    if (s.output_bus_mask == 0) s.output_bus_mask = 0x01;
    s.output_bus_mask &= (~s.output_bus_mask + 1u);
    s.restart_count = std::clamp(s.restart_count, 0,
                                 std::numeric_limits<int>::max());
    return s;
}

void save(const AppSettings& s) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, appinfo::kRegistryPath, 0, nullptr,
                        0, KEY_WRITE, nullptr, &key, nullptr) !=
        ERROR_SUCCESS) {
        return;
    }

    auto write_dword = [&](const wchar_t* name, DWORD v) {
        RegSetValueExW(key, name, 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&v), sizeof(v));
    };

    write_dword(L"Enabled", s.enabled ? 1 : 0);
    write_dword(L"PlayThresholdDb",
                static_cast<DWORD>(static_cast<long>(s.play_threshold_db * 100.0f)));
    write_dword(L"SilenceThresholdDb",
                static_cast<DWORD>(static_cast<long>(s.silence_threshold_db * 100.0f)));
    write_dword(L"ConfirmSamples", static_cast<DWORD>(s.confirm_samples));
    write_dword(L"ArmAfterMs", static_cast<DWORD>(s.arm_after_ms));
    write_dword(L"CooldownMs", static_cast<DWORD>(s.cooldown_ms));
    write_dword(L"OutputBusMask", s.output_bus_mask & 0x1f);
    write_dword(L"StartMinimized", s.start_minimized ? 1 : 0);
    write_dword(L"RestartCount", static_cast<DWORD>(s.restart_count));
    RegSetValueExW(key, L"LastRestart", 0, REG_SZ,
                   reinterpret_cast<const BYTE*>(s.last_restart.c_str()),
                   static_cast<DWORD>((s.last_restart.size() + 1) * sizeof(wchar_t)));

    RegCloseKey(key);
}

bool get_autostart() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunPath, 0, KEY_READ, &key) !=
        ERROR_SUCCESS) {
        return false;
    }
    DWORD size = 0;
    LONG rc = RegQueryValueExW(key, appinfo::kRunValue, nullptr, nullptr,
                               nullptr, &size);
    RegCloseKey(key);
    return rc == ERROR_SUCCESS;
}

void set_autostart(bool enabled) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunPath, 0, nullptr, 0, KEY_SET_VALUE,
                        nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return;
    }
    if (enabled) {
        std::wstring cmd = L"\"" + current_exe_path() + L"\"";
        RegSetValueExW(key, appinfo::kRunValue, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(cmd.c_str()),
                       static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, appinfo::kRunValue);
    }
    RegCloseKey(key);
}

} // namespace settings
} // namespace vmwake
