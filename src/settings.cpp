#include "settings.hpp"

#include <windows.h>
#include <algorithm>
#include <vector>

#include "app_info.hpp"

namespace vmwake {
namespace settings {

namespace {

constexpr wchar_t kRunPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

std::wstring current_exe_path() {
    std::vector<wchar_t> buf(MAX_PATH);
    while (buf.size() <= 32768) {
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
    return s;
}

bool save(const AppSettings& s) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, appinfo::kRegistryPath, 0, nullptr,
                        0, KEY_WRITE, nullptr, &key, nullptr) !=
        ERROR_SUCCESS) {
        return false;
    }

    auto write_dword = [&](const wchar_t* name, DWORD v) {
        return RegSetValueExW(key, name, 0, REG_DWORD,
                              reinterpret_cast<const BYTE*>(&v), sizeof(v)) ==
               ERROR_SUCCESS;
    };

    std::uint32_t output_mask = s.output_bus_mask & 0x1f;
    if (output_mask == 0) output_mask = 0x01;
    output_mask &= (~output_mask + 1u);

    bool ok = true;
    ok &= write_dword(L"Enabled", s.enabled ? 1 : 0);
    ok &= write_dword(L"PlayThresholdDb",
                      static_cast<DWORD>(static_cast<long>(s.play_threshold_db * 100.0f)));
    ok &= write_dword(L"SilenceThresholdDb",
                      static_cast<DWORD>(static_cast<long>(s.silence_threshold_db * 100.0f)));
    ok &= write_dword(L"ConfirmSamples", static_cast<DWORD>(s.confirm_samples));
    ok &= write_dword(L"ArmAfterMs", static_cast<DWORD>(s.arm_after_ms));
    ok &= write_dword(L"CooldownMs", static_cast<DWORD>(s.cooldown_ms));
    ok &= write_dword(L"OutputBusMask", output_mask);
    ok &= write_dword(L"StartMinimized", s.start_minimized ? 1 : 0);

    RegCloseKey(key);
    return ok;
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

bool set_autostart(bool enabled) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunPath, 0, nullptr, 0, KEY_SET_VALUE,
                        nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return false;
    }
    bool ok = false;
    if (enabled) {
        const std::wstring exe_path = current_exe_path();
        if (!exe_path.empty()) {
            const std::wstring cmd = L"\"" + exe_path + L"\"";
            ok = RegSetValueExW(key, appinfo::kRunValue, 0, REG_SZ,
                                 reinterpret_cast<const BYTE*>(cmd.c_str()),
                                 static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t))) ==
                 ERROR_SUCCESS;
        }
    } else {
        const LONG rc = RegDeleteValueW(key, appinfo::kRunValue);
        ok = rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND;
    }
    RegCloseKey(key);
    return ok;
}

} // namespace settings
} // namespace vmwake
