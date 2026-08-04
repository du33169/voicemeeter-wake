#pragma once

#include <windows.h>
#include <shellapi.h>

#include <deque>
#include <string>

#include "audio_monitor.hpp"
#include "main_window_ids.hpp"
#include "settings.hpp"
#include "voicemeeter_remote.hpp"

namespace vmwake {

LRESULT CALLBACK MainWindowProc(HWND, UINT, WPARAM, LPARAM);

// Main window: status area, settings, stats, tray icon, and an operation log.
class MainWindow {
public:

    static constexpr UINT kTrayMsg = WM_APP + 1;
    static constexpr UINT_PTR kLevelBarSubclassId = 1;

    static LRESULT CALLBACK LevelBarSubclassProc(HWND, UINT, WPARAM, LPARAM,
                                                 UINT_PTR, DWORD_PTR);

    MainWindow() = default;
    ~MainWindow();

    bool create(HINSTANCE hInstance);
    void run_message_loop();
    HWND handle() const { return hwnd_; }

private:
    friend LRESULT CALLBACK MainWindowProc(HWND, UINT, WPARAM, LPARAM);

    LRESULT handle_message(UINT msg, WPARAM wParam, LPARAM lParam);
    void on_create();
    void on_timer();
    void on_command(int id);
    void on_notify(LPARAM lParam);
    void on_close();
    void on_destroy();
    void on_tray(WPARAM wParam, LPARAM lParam);

    // ui
    void create_controls();
    void apply_settings_to_controls();
    void apply_controls_to_settings();
    void refresh_status_texts();
    void set_status_text(const std::wstring& text);
    void set_state_text(const std::wstring& text);
    void append_log(const std::wstring& text);
    void update_output_controls();
    void update_pause_button();
    int output_count() const;
    std::uint32_t selected_output_mask() const;
    std::wstring selected_outputs_text() const;
    static std::wstring state_text(MonitorState s);
    static std::wstring now_timestamp();
    std::wstring vm_product_name() const;

    // monitoring
    bool start_timer();
    void poll_voicemeeter();
    void attempt_connect();
    void reset_connection();
    void feed_monitor(float peak_db, std::int64_t now_ms, bool api_ok);
    void update_level_display(float peak_db);
    bool do_restart();
    LRESULT level_bar_paint(HWND hwnd);

    // tray
    bool create_tray_icon();
    void remove_tray_icon();
    void update_tray_tooltip();

    HWND control(int id) const;
    void apply_font(HWND hwnd);

    HINSTANCE hinstance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND hwnd_log_ = nullptr;
    HWND hwnd_level_bar_ = nullptr;
    HWND hwnd_status_ = nullptr;
    HWND hwnd_state_ = nullptr;
    HWND hwnd_silence_ = nullptr;
    HWND hwnd_level_text_ = nullptr;
    HWND hwnd_restart_count_ = nullptr;
    HWND hwnd_last_restart_ = nullptr;
    HWND hwnd_play_spin_ = nullptr;
    HWND hwnd_sil_spin_ = nullptr;
    HWND hwnd_arm_spin_ = nullptr;
    HWND hwnd_cooldown_spin_ = nullptr;
    HWND hwnd_outputs_label_ = nullptr;
    HWND hwnd_vm_version_ = nullptr;
    HWND hwnd_remote_dll_ = nullptr;

    bool hidden_ = false;
    bool tray_icon_added_ = false;
    bool tray_uses_v4_ = false;
    bool logged_in_ = false;
    bool type_known_ = false;
    bool version_known_ = false;
    bool vm_running_ = false;
    unsigned long vm_version_ = 0;
    VoicemeeterRemote::Type vm_type_ = VoicemeeterRemote::Type::None;

    AppSettings settings_;
    MonitorConfig monitor_cfg_;
    AudioMonitor monitor_;
    VoicemeeterRemote remote_;

    UINT timer_id_ = 0;
    UINT taskbar_created_msg_ = 0;
    UINT dpi_ = 96;
    HFONT font_ = nullptr;
    HFONT bold_font_ = nullptr;

    std::int64_t last_now_ms_ = 0;
    std::int64_t last_load_try_ms_ = 0;
    std::int64_t last_login_try_ms_ = 0;
    int tick_ = 0;
    float last_peak_db_ = -120.0f;

    NOTIFYICONDATAW nid_{};
    std::deque<std::wstring> log_lines_;
};

} // namespace vmwake
