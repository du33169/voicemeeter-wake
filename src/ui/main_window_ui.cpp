#include "main_window.hpp"

#include <windowsx.h>
#include <commctrl.h>

#include <algorithm>
#include <cstdio>
#include <cmath>

#include "logger.hpp"
#include "win_util.hpp"

namespace vmwake {

namespace {

constexpr int kSliderMinDb = -60;
constexpr int kSliderMaxDb = 0;
constexpr int kArmMaxMin = 120;     // arm silence: 1..120 minutes
constexpr int kCooldownMaxSec = 60; // cooldown: 1..60 seconds

} // namespace

void MainWindow::create_controls() {
    auto S = [this](int v) { return dpi_scale(v, dpi_); };

    auto mk = [&](const wchar_t* cls, const wchar_t* text, DWORD style,
                  int x, int y, int w, int height, int id) -> HWND {
        HWND hw = CreateWindowExW(0, cls, text, style, S(x), S(y), S(w),
                                  S(height), hwnd_,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                  hinstance_, nullptr);
        apply_font(hw);
        return hw;
    };
    auto mkLabel = [&](const wchar_t* text, int x, int y, int w, int h,
                       int id) {
        return mk(L"STATIC", text,
                  WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, x, y, w, h,
                  id);
    };
    auto mkSlider = [&](int x, int y, int w, int min, int max, int id) {
        HWND tb = mk(TRACKBAR_CLASS, L"",
                     WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ, x, y, w,
                     22, id);
        SendMessageW(tb, TBM_SETRANGEMIN, TRUE, min);
        SendMessageW(tb, TBM_SETRANGEMAX, TRUE, max);
        SendMessageW(tb, TBM_SETPAGESIZE, 0, 5);
        SendMessageW(tb, TBM_SETTICFREQ, 5, 0);
        return tb;
    };

    const int L = 16;
    const int W = 552;

    // --- Voicemeeter status group ---
    mk(L"BUTTON", L"Voicemeeter status", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
       L, 8, W, 118, 0);
    mkLabel(L"Status:", L + 10, 30, 70, 20, 0);
    hwnd_status_ = mkLabel(L"Not connected", L + 80, 30, W - 90, 20,
                           IDC_STATIC_STATUS);

    mkLabel(L"A1 level:", L + 10, 58, 80, 20, 0);
    hwnd_level_bar_ = mk(L"msctls_progress32", L"",
                         WS_CHILD | WS_VISIBLE | PBS_SMOOTH, L + 90, 60,
                         W - 200, 16, IDC_PROGRESS_LEVEL);
    SendMessageW(hwnd_level_bar_, PBM_SETRANGE32, 0, 1000);
    hwnd_level_text_ =
        mkLabel(L"-- dB", L + W - 100, 58, 90, 20, IDC_STATIC_LEVEL);

    mkLabel(L"Monitor:", L + 10, 86, 80, 20, 0);
    hwnd_state_ = mkLabel(L"Not connected", L + 90, 86, W - 200, 20,
                          IDC_STATIC_STATE);
    mkLabel(L"Silent:", L + W - 190, 86, 60, 20, 0);
    hwnd_silence_ =
        mkLabel(L"0 sec", L + W - 130, 86, 120, 20, IDC_STATIC_SILENCE);

    // --- Action buttons ---
    mk(L"BUTTON", L"Launch Voicemeeter", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
           BS_PUSHBUTTON, L + 10, 132, 170, 28, IDC_BTN_START_VM);
    mk(L"BUTTON", L"Restart engine now", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
           BS_PUSHBUTTON, L + 190, 132, 170, 28, IDC_BTN_RESTART);

    // --- Settings group (all options together) ---
    const int sy = 176;
    const int sh = 206;
    mk(L"BUTTON", L"Settings", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, L, sy, W,
       sh, 0);
    mk(L"BUTTON", L"Enable auto-wake", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
           BS_AUTOCHECKBOX, L + 10, sy + 22, 220, 22, IDC_CHK_ENABLED);

    mkLabel(L"Play thr (dB):", L + 10, sy + 50, 96, 20, 0);
    mkSlider(L + 110, sy + 48, W - 236, kSliderMinDb, kSliderMaxDb,
             IDC_SLIDER_PLAY);
    hwnd_play_val_ =
        mkLabel(L"-50 dB", L + W - 120, sy + 50, 110, 20, IDC_STATIC_PLAY_VAL);

    mkLabel(L"Silence thr (dB):", L + 10, sy + 80, 100, 20, 0);
    mkSlider(L + 110, sy + 78, W - 236, kSliderMinDb, kSliderMaxDb,
             IDC_SLIDER_SIL);
    hwnd_sil_val_ =
        mkLabel(L"-55 dB", L + W - 120, sy + 80, 110, 20, IDC_STATIC_SIL_VAL);

    mkLabel(L"Arm silence (min):", L + 10, sy + 110, 108, 20, 0);
    mkSlider(L + 122, sy + 108, W - 246, 1, kArmMaxMin, IDC_SLIDER_ARM);
    hwnd_arm_val_ =
        mkLabel(L"10 min", L + W - 120, sy + 110, 110, 20, IDC_STATIC_ARM_VAL);

    mkLabel(L"Cooldown (sec):", L + 10, sy + 140, 104, 20, 0);
    mkSlider(L + 122, sy + 138, W - 246, 1, kCooldownMaxSec, IDC_SLIDER_COOLDOWN);
    hwnd_cooldown_val_ = mkLabel(L"5 sec", L + W - 120, sy + 140, 110, 20,
                                 IDC_STATIC_COOLDOWN_VAL);

    mk(L"BUTTON", L"Run at startup", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
           BS_AUTOCHECKBOX, L + 10, sy + 170, 190, 22, IDC_CHK_AUTOSTART);
    mk(L"BUTTON", L"Start minimized to tray", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
           BS_AUTOCHECKBOX, L + 220, sy + 170, 230, 22, IDC_CHK_MINIMIZE);
    mk(L"BUTTON", L"Save settings", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
           BS_PUSHBUTTON, L + 430, sy + 168, 100, 26, IDC_BTN_SAVE);

    // --- Stats ---
    const int statY = sy + sh + 12;
    mk(L"BUTTON", L"Stats", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, L, statY, W,
       74, 0);
    mkLabel(L"Restarts:", L + 10, statY + 22, 90, 20, 0);
    hwnd_restart_count_ = mkLabel(L"0", L + 100, statY + 22, 120, 20,
                                  IDC_STATIC_RESTART_COUNT);
    mkLabel(L"Last restart:", L + 10, statY + 44, 90, 20, 0);
    hwnd_last_restart_ = mkLabel(L"Never", L + 100, statY + 44, W - 110, 20,
                                 IDC_STATIC_LAST_RESTART);

    // --- Operation log ---
    const int logY = statY + 74 + 10;
    mk(L"BUTTON", L"Operation log", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, L,
       logY, W, 256, 0);
    mk(L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY |
           WS_VSCROLL | WS_TABSTOP | ES_AUTOVSCROLL,
       L + 10, logY + 20, W - 20, 226, IDC_EDIT_LOG);
}

void MainWindow::on_slider(HWND trackbar) {
    const int id = GetDlgCtrlID(trackbar);
    const int pos = static_cast<int>(SendMessageW(trackbar, TBM_GETPOS, 0, 0));
    wchar_t buf[24];
    switch (id) {
    case IDC_SLIDER_PLAY:
        swprintf(buf, 24, L"%d dB", pos);
        SetWindowTextW(hwnd_play_val_, buf);
        break;
    case IDC_SLIDER_SIL:
        swprintf(buf, 24, L"%d dB", pos);
        SetWindowTextW(hwnd_sil_val_, buf);
        break;
    case IDC_SLIDER_ARM:
        swprintf(buf, 24, L"%d min", pos);
        SetWindowTextW(hwnd_arm_val_, buf);
        break;
    case IDC_SLIDER_COOLDOWN:
        swprintf(buf, 24, L"%d sec", pos);
        SetWindowTextW(hwnd_cooldown_val_, buf);
        break;
    default:
        break;
    }
}

void MainWindow::apply_settings_to_controls() {
    Button_SetCheck(control(IDC_CHK_ENABLED),
                    settings_.enabled ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(control(IDC_CHK_MINIMIZE),
                    settings_.start_minimized ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(control(IDC_CHK_AUTOSTART),
                    settings::get_autostart() ? BST_CHECKED : BST_UNCHECKED);

    wchar_t buf[48];

    int db = std::clamp(static_cast<int>(std::lround(settings_.play_threshold_db)),
                        kSliderMinDb, kSliderMaxDb);
    SendMessageW(control(IDC_SLIDER_PLAY), TBM_SETPOS, TRUE, db);
    swprintf(buf, 48, L"%d dB", db);
    SetWindowTextW(hwnd_play_val_, buf);

    db = std::clamp(static_cast<int>(std::lround(settings_.silence_threshold_db)),
                    kSliderMinDb, kSliderMaxDb);
    SendMessageW(control(IDC_SLIDER_SIL), TBM_SETPOS, TRUE, db);
    swprintf(buf, 48, L"%d dB", db);
    SetWindowTextW(hwnd_sil_val_, buf);

    int min = std::clamp(static_cast<int>(std::lround(settings_.arm_after_ms / 60000.0)),
                         1, kArmMaxMin);
    SendMessageW(control(IDC_SLIDER_ARM), TBM_SETPOS, TRUE, min);
    swprintf(buf, 48, L"%d min", min);
    SetWindowTextW(hwnd_arm_val_, buf);

    int sec = std::clamp(static_cast<int>(std::lround(settings_.cooldown_ms / 1000.0)),
                         1, kCooldownMaxSec);
    SendMessageW(control(IDC_SLIDER_COOLDOWN), TBM_SETPOS, TRUE, sec);
    swprintf(buf, 48, L"%d sec", sec);
    SetWindowTextW(hwnd_cooldown_val_, buf);

    swprintf(buf, 48, L"%d", settings_.restart_count);
    SetWindowTextW(hwnd_restart_count_, buf);
    SetWindowTextW(hwnd_last_restart_,
                   settings_.last_restart.empty() ? L"Never"
                                                  : settings_.last_restart.c_str());
}

void MainWindow::apply_controls_to_settings() {
    settings_.enabled = Button_GetCheck(control(IDC_CHK_ENABLED)) == BST_CHECKED;
    settings_.start_minimized =
        Button_GetCheck(control(IDC_CHK_MINIMIZE)) == BST_CHECKED;

    const int pp =
        static_cast<int>(SendMessageW(control(IDC_SLIDER_PLAY), TBM_GETPOS, 0, 0));
    settings_.play_threshold_db = static_cast<float>(pp);
    int sp =
        static_cast<int>(SendMessageW(control(IDC_SLIDER_SIL), TBM_GETPOS, 0, 0));
    settings_.silence_threshold_db = static_cast<float>(sp);

    // Keep a hysteresis gap: silence must stay below the play threshold.
    if (settings_.silence_threshold_db >= settings_.play_threshold_db) {
        settings_.silence_threshold_db = settings_.play_threshold_db - 5.0f;
        sp = static_cast<int>(std::lround(settings_.silence_threshold_db));
        SendMessageW(control(IDC_SLIDER_SIL), TBM_SETPOS, TRUE, sp);
        wchar_t b[16];
        swprintf(b, 16, L"%d dB", sp);
        SetWindowTextW(hwnd_sil_val_, b);
    }

    const int arm_min =
        static_cast<int>(SendMessageW(control(IDC_SLIDER_ARM), TBM_GETPOS, 0, 0));
    settings_.arm_after_ms = std::clamp(arm_min, 1, kArmMaxMin) * 60000;
    const int cool_sec = static_cast<int>(
        SendMessageW(control(IDC_SLIDER_COOLDOWN), TBM_GETPOS, 0, 0));
    settings_.cooldown_ms = std::clamp(cool_sec, 1, kCooldownMaxSec) * 1000;
}

void MainWindow::refresh_status_texts() {
    wchar_t buf[64];
    if (!remote_.loaded()) {
        set_status_text(L"Voicemeeter Remote not found (not installed?)");
    } else if (!logged_in_) {
        set_status_text(L"Connecting to Voicemeeter...");
    } else if (!type_known_) {
        set_status_text(L"Connected; waiting for version info...");
    } else {
        switch (vm_type_) {
        case VoicemeeterRemote::Type::Banana:
            swprintf(buf, 64, L"Connected: Voicemeeter Banana");
            break;
        case VoicemeeterRemote::Type::Voicemeeter:
            swprintf(buf, 64, L"Connected: Voicemeeter (Banana required)");
            break;
        case VoicemeeterRemote::Type::Potato:
        case VoicemeeterRemote::Type::PotatoX64:
            swprintf(buf, 64,
                     L"Connected: Voicemeeter Potato (Banana required)");
            break;
        default:
            swprintf(buf, 64, L"Unknown version");
            break;
        }
        set_status_text(buf);
    }
    update_tray_tooltip();
}

void MainWindow::set_status_text(const std::wstring& text) {
    SetWindowTextW(hwnd_status_, text.c_str());
}

void MainWindow::set_state_text(const std::wstring& text) {
    SetWindowTextW(hwnd_state_, text.c_str());
}

std::wstring MainWindow::state_text(MonitorState s) {
    switch (s) {
    case MonitorState::Disconnected:
        return L"Not connected";
    case MonitorState::WaitingSilence:
        return L"Waiting for silence...";
    case MonitorState::TrackingSilence:
        return L"Tracking silence...";
    case MonitorState::Armed:
        return L"Armed (waiting for playback)";
    case MonitorState::Cooldown:
        return L"Cooldown...";
    }
    return L"";
}

std::wstring MainWindow::now_timestamp() { return ftime(); }

void MainWindow::append_log(const std::wstring& text) {
    const std::wstring line = ftime() + L"  " + text;
    Logger::instance().write(line);
    log_lines_.push_back(line);
    if (log_lines_.size() > 300) {
        log_lines_.pop_front();
    }
    std::wstring all;
    for (const auto& l : log_lines_) {
        all += l;
        all += L"\r\n";
    }
    SetWindowTextW(hwnd_log_, all.c_str());
    SendMessageW(hwnd_log_, EM_SETSEL, WPARAM(-1), LPARAM(-1));
    SendMessageW(hwnd_log_, EM_SCROLLCARET, 0, 0);
}

} // namespace vmwake
