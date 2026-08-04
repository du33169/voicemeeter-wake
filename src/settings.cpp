#include "settings.hpp"

#include <windows.h>
#include <vector>

namespace vmwake {
namespace settings {

namespace {

constexpr wchar_t kRegPath[] = L"Software\\VoicemeeterEngineWake";
constexpr wchar_t kRunPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValue[] = L"VoicemeeterEngineWake";

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

} // namespace

const wchar_t* registry_path() { return kRegPath; }

AppSettings load() {
    AppSettings s;

    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegPath, 0, KEY_READ, &key) !=
        ERROR_SUCCESS) {
        return s;
    }

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
    if (read_dword(L"ConfirmSamples", &v)) s.confirm_samples = static_cast<int>(v);
    if (read_dword(L"ArmAfterMs", &v)) s.arm_after_ms = static_cast<int>(v);
    if (read_dword(L"CooldownMs", &v)) s.cooldown_ms = static_cast<int>(v);
    if (read_dword(L"StartMinimized", &v)) s.start_minimized = v != 0;
    if (read_dword(L"RestartCount", &v)) s.restart_count = static_cast<int>(v);
    read_str(L"LastRestart", &s.last_restart);

    RegCloseKey(key);
    return s;
}

void save(const AppSettings& s) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegPath, 0, nullptr, 0,
                        KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
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
    LONG rc = RegQueryValueExW(key, kRunValue, nullptr, nullptr, nullptr, &size);
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
        RegSetValueExW(key, kRunValue, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(cmd.c_str()),
                       static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, kRunValue);
    }
    RegCloseKey(key);
}

} // namespace settings
} // namespace vmwake
