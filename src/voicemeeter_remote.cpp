#include "voicemeeter_remote.hpp"

#include <algorithm>
#include <cstdio>
#include <cwctype>
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

    // UninstallString commonly quotes the executable and appends arguments.
    std::wstring command = buf;
    const size_t first = command.find_first_not_of(L" \t");
    if (first == std::wstring::npos) return {};
    command.erase(0, first);
    if (command.front() == L'\"') {
        const size_t closing = command.find(L'\"', 1);
        if (closing == std::wstring::npos) return {};
        command.resize(closing);
        command.erase(0, 1);
    } else {
        const auto exe = std::search(command.begin(), command.end(),
                                     L".exe", L".exe" + 4,
                                     [](wchar_t a, wchar_t b) {
                                         return std::towlower(a) == b;
                                     });
        if (exe != command.end()) command.resize((exe - command.begin()) + 4);
    }

    // UninstallString points at Voicemeeter*.exe; strip the file name.
    std::wstring dir = command;
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
    module_path_ = dll;

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
    module_path_.clear();
}

long VoicemeeterRemote::login() {
    if (!fn_login_) return -1;
    const long rc = fn_login_();
    if (rc == 0 || rc == 1) logged_in_ = true;
    return rc;
}

long VoicemeeterRemote::logout() {
    if (!logged_in_) return 0;
    return force_logout();
}

long VoicemeeterRemote::force_logout() {
    const long rc = fn_logout_ ? fn_logout_() : -1;
    logged_in_ = false;
    return rc;
}

long VoicemeeterRemote::run_voicemeeter(Type type) {
    return fn_run_ ? fn_run_(static_cast<long>(type)) : -1;
}

long VoicemeeterRemote::get_type(Type& out) {
    if (!fn_get_type_) return -1;
    long t = 0;
    const long rc = fn_get_type_(&t);
    if (rc != 0) return rc;
    out = static_cast<Type>(t);
    return 0;
}

long VoicemeeterRemote::get_version(unsigned long& out) {
    if (!fn_get_version_) return -1;
    long v = 0;
    const long rc = fn_get_version_(&v);
    if (rc != 0) return rc;
    out = static_cast<unsigned long>(v);
    return 0;
}

long VoicemeeterRemote::get_level(long levelType, long channel, float& out) {
    if (!fn_get_level_) return -1;
    return fn_get_level_(levelType, channel, &out);
}

long VoicemeeterRemote::set_parameter_float(const char* name, float value) {
    if (!fn_set_parameter_float_) return -1;
    return fn_set_parameter_float_(const_cast<char*>(name), value);
}

const wchar_t* VoicemeeterRemote::login_result_text(long rc) {
    switch (rc) {
    case 0: return L"OK";
    case 1: return L"Voicemeeter is not running";
    case -1: return L"cannot get Remote API client";
    case -2: return L"unexpected login; logout required";
    default: return L"unknown result";
    }
}

const wchar_t* VoicemeeterRemote::info_result_text(long rc) {
    switch (rc) {
    case 0: return L"OK";
    case -1: return L"cannot get Remote API client";
    case -2: return L"Voicemeeter server unavailable";
    default: return L"unknown result";
    }
}

const wchar_t* VoicemeeterRemote::level_result_text(long rc) {
    switch (rc) {
    case 0: return L"OK";
    case -1: return L"Remote API error";
    case -2: return L"Voicemeeter server unavailable";
    case -3: return L"level temporarily unavailable";
    case -4: return L"level channel out of range";
    default: return L"unknown result";
    }
}

const wchar_t* VoicemeeterRemote::set_parameter_result_text(long rc) {
    switch (rc) {
    case 0: return L"OK";
    case -1: return L"Remote API error";
    case -2: return L"Voicemeeter server unavailable";
    case -3: return L"unknown parameter";
    default: return L"unknown result";
    }
}

} // namespace vmwake
