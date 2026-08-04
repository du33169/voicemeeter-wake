#include "voicemeeter_remote.hpp"

#include <cstdio>
#include <vector>

// GetProcAddress returns FARPROC; casting it to the real function-pointer type
// is the standard dynamic-loading pattern.
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

namespace vmwake {

namespace {

constexpr wchar_t kUninstallKey[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
    L"VB:Voicemeeter {17359A74-1236-5467}";

// Returns the Voicemeeter install directory (with trailing backslash) or empty.
std::wstring find_install_dir() {
    HKEY key = nullptr;
    // Try the native 64-bit view first, then the 32-bit view.
    for (DWORD view : {KEY_WOW64_64KEY, KEY_WOW64_32KEY}) {
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, kUninstallKey, 0,
                          KEY_READ | view, &key) == ERROR_SUCCESS) {
            break;
        }
        key = nullptr;
    }
    if (!key) {
        return {};
    }

    wchar_t buf[MAX_PATH] = {0};
    DWORD size = sizeof(buf);
    DWORD type = 0;
    LSTATUS rc = RegQueryValueExW(key, L"UninstallString", nullptr, &type,
                                  reinterpret_cast<LPBYTE>(buf), &size);
    RegCloseKey(key);
    if (rc != ERROR_SUCCESS || type != REG_SZ) {
        return {};
    }

    // UninstallString points at Voicemeeter*.exe; strip the file name.
    std::wstring dir = buf;
    const size_t slash = dir.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return {};
    }
    dir.resize(slash + 1);
    return dir;
}

} // namespace

bool VoicemeeterRemote::load() {
    unload();

    const std::wstring dir = find_install_dir();
    if (dir.empty()) {
        last_error_ = L"Voicemeeter is not installed (uninstall key not found in registry)";
        return false;
    }

    std::wstring dll = dir + L"VoicemeeterRemote64.dll";
    module_ = LoadLibraryW(dll.c_str());
    if (!module_) {
        last_error_ = L"Failed to load " + dll;
        return false;
    }

    fn_login_ = reinterpret_cast<FnLong>(GetProcAddress(module_, "VBVMR_Login"));
    fn_logout_ = reinterpret_cast<FnLong>(GetProcAddress(module_, "VBVMR_Logout"));
    fn_run_ = reinterpret_cast<FnLongLong>(GetProcAddress(module_, "VBVMR_RunVoicemeeter"));
    fn_get_type_ = reinterpret_cast<FnTypeLong>(GetProcAddress(module_, "VBVMR_GetVoicemeeterType"));
    fn_get_version_ = reinterpret_cast<FnTypeLong>(GetProcAddress(module_, "VBVMR_GetVoicemeeterVersion"));
    fn_get_level_ = reinterpret_cast<FnLevel>(GetProcAddress(module_, "VBVMR_GetLevel"));
    fn_set_parameter_float_ = reinterpret_cast<FnSetFloat>(GetProcAddress(module_, "VBVMR_SetParameterFloat"));

    if (!fn_login_ || !fn_logout_ || !fn_run_ || !fn_get_type_ || !fn_get_version_ ||
        !fn_get_level_ || !fn_set_parameter_float_) {
        last_error_ = L"VoicemeeterRemote64.dll is missing required exports";
        unload();
        return false;
    }

    last_error_.clear();
    return true;
}

void VoicemeeterRemote::unload() {
    if (logged_in_) logout();
    if (module_) {
        FreeLibrary(module_);
        module_ = nullptr;
    }
    fn_login_ = nullptr;
    fn_logout_ = nullptr;
    fn_run_ = nullptr;
    fn_get_type_ = nullptr;
    fn_get_version_ = nullptr;
    fn_get_level_ = nullptr;
    fn_set_parameter_float_ = nullptr;
    logged_in_ = false;
}

long VoicemeeterRemote::login() {
    if (!fn_login_) return -1;
    const long rc = fn_login_();
    if (rc == 0 || rc == 1) logged_in_ = true;
    return rc;
}

long VoicemeeterRemote::logout() {
    if (!logged_in_) return 0;
    const long rc = fn_logout_ ? fn_logout_() : -1;
    logged_in_ = false;
    return rc;
}

long VoicemeeterRemote::run_voicemeeter(Type type) {
    return fn_run_ ? fn_run_(static_cast<long>(type)) : -1;
}

bool VoicemeeterRemote::get_type(Type& out) {
    if (!fn_get_type_) return false;
    long t = 0;
    if (fn_get_type_(&t) != 0) return false;
    out = static_cast<Type>(t);
    return true;
}

bool VoicemeeterRemote::get_version(unsigned long& out) {
    if (!fn_get_version_) return false;
    long v = 0;
    if (fn_get_version_(&v) != 0) return false;
    out = static_cast<unsigned long>(v);
    return true;
}

bool VoicemeeterRemote::get_level(long levelType, long channel, float& out) {
    if (!fn_get_level_) return false;
    return fn_get_level_(levelType, channel, &out) == 0;
}

bool VoicemeeterRemote::set_parameter_float(const char* name, float value) {
    if (!fn_set_parameter_float_) return false;
    return fn_set_parameter_float_(const_cast<char*>(name), value) == 0;
}

} // namespace vmwake
