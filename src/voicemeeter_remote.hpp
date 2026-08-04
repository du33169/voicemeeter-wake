#pragma once

#include <windows.h>
#include <string>

namespace vmwake {

// Thin dynamic loader for the official Voicemeeter Remote API.
// The VoicemeeterRemote64.dll is installed with Voicemeeter; we never
// redistribute it.  All callers must be on a single thread (the API is not
// thread-safe and GetLevel/IsParametersDirty are documented as single-thread).
class VoicemeeterRemote {
public:
    enum class Type {
        None = 0,
        Voicemeeter = 1,
        Banana = 2,
        Potato = 3,
        PotatoX64 = 6,
    };

    VoicemeeterRemote() = default;
    ~VoicemeeterRemote() { unload(); }
    VoicemeeterRemote(const VoicemeeterRemote&) = delete;
    VoicemeeterRemote& operator=(const VoicemeeterRemote&) = delete;

    // Locate the Voicemeeter install dir in the registry and LoadLibrary the
    // 64-bit Remote DLL.  Returns true on success (even if Voicemeeter itself
    // is not currently running).
    bool load();

    void unload();
    bool loaded() const { return module_ != nullptr; }

    const std::wstring& last_error() const { return last_error_; }

    // VBVMR_Login: 0 = ok, 1 = ok but Voicemeeter not running.
    long login();
    long logout();
    long run_voicemeeter(Type type);
    bool get_type(Type& out);
    bool get_version(unsigned long& out);
    bool get_level(long levelType, long channel, float& out);
    bool set_parameter_float(const char* name, float value);

private:
    using FnLong = long(WINAPI*)();
    using FnLongLong = long(WINAPI*)(long);
    using FnTypeLong = long(WINAPI*)(long*);
    using FnLevel = long(WINAPI*)(long, long, float*);
    using FnSetFloat = long(WINAPI*)(char*, float);

    HMODULE module_ = nullptr;
    bool logged_in_ = false;
    std::wstring last_error_;

    FnLong fn_login_ = nullptr;
    FnLong fn_logout_ = nullptr;
    FnLongLong fn_run_ = nullptr;
    FnTypeLong fn_get_type_ = nullptr;
    FnTypeLong fn_get_version_ = nullptr;
    FnLevel fn_get_level_ = nullptr;
    FnSetFloat fn_set_parameter_float_ = nullptr;
};

} // namespace vmwake
