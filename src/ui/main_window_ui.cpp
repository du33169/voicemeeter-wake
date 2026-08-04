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
constexpr int kPlaySliderMinDb = -55;
constexpr int kSliderMaxDb = 0;
constexpr int kArmMaxMin = 120;     // arm silence: 1..120 minutes
constexpr int kCooldownMaxSec = 60; // cooldown: 1..60 seconds

} // namespace

void MainWindow::create_controls() {
    auto S = [this](int v) { return dpi_scale(v, dpi_); };

    auto mkEx = [&](DWORD ex_style, const wchar_t* cls, const wchar_t* text,
                    DWORD style, int x, int y, int w, int height,
                    int id) -> HWND {
        HWND hw = CreateWindowExW(ex_style, cls, text, style, S(x), S(y), S(w),
                                  S(height), hwnd_,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                  hinstance_, nullptr);
        apply_font(hw);
        return hw;
    };
    auto mk = [&](const wchar_t* cls, const wchar_t* text, DWORD style,
                  int x, int y, int w, int height, int id) -> HWND {
        return mkEx(0, cls, text, style, x, y, w, height, id);
    };
    auto mkLabel = [&](const wchar_t* text, int x, int y, int w, int h,
                       int id) {
        return mk(L"STATIC", text,
                  WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, x, y, w, h,
                  id);
    };
    const int L = 16;
    const int W = 552;

    // --- Voicemeeter status group ---
    mk(L"BUTTON", L"Voicemeeter status", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
       L, 8, W, 118, 0);
    mkLabel(L"Status:", L + 10, 30, 70, 20, 0);
    hwnd_status_ = mkLabel(L"Not connected", L + 80, 30, W - 90, 20,
                            IDC_STATIC_STATUS);

    mkLabel(L"Selected level:", L + 10, 58, 90, 20, 0);
    hwnd_level_bar_ = mk(L"msctls_progress32", L"",
                         WS_CHILD | WS_VISIBLE | PBS_SMOOTH, L + 100, 60,
                         W - 210, 16, IDC_PROGRESS_LEVEL);
    SendMessageW(hwnd_level_bar_, PBM_SETRANGE32, 0, 1000);
    hwnd_level_text_ =
        mkLabel(L"-- dB", L + W - 100, 58, 90, 20, IDC_STATIC_LEVEL);

    mkLabel(L"Monitor:", L + 10, 86, 80, 20, 0);
    hwnd_state_ = mkLabel(L"Not connected", L + 80, 86, 210, 20,
                           IDC_STATIC_STATE);
    mkLabel(L"Silent:", L + 300, 86, 50, 20, 0);
    hwnd_silence_ =
        mkLabel(L"0 sec", L + 350, 86, 70, 20, IDC_STATIC_SILENCE);
    mk(L"BUTTON", L"Pause", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
       L + 430, 82, 110, 26, IDC_BTN_TOGGLE);

    // --- Action buttons ---
    mk(L"BUTTON", L"Launch Voicemeeter", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
           BS_PUSHBUTTON, L + 10, 132, 160, 28, IDC_BTN_START_VM);
    mk(L"BUTTON", L"Restart engine now", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
           BS_PUSHBUTTON, L + 180, 132, 160, 28, IDC_BTN_RESTART);
    mk(L"BUTTON", L"Exit", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
           BS_PUSHBUTTON, L + 350, 132, 100, 28, IDC_BTN_EXIT);

    // --- Settings group (all options together) ---
    const int sy = 176;
    const int sh = 206;
    mk(L"BUTTON", L"Settings", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, L, sy, W,
       sh, 0);
    mkLabel(L"Play thr (dB):", L + 10, sy + 22, 96, 20, 0);
    mkEx(WS_EX_CLIENTEDGE, L"EDIT", L"-50",
         WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_RIGHT, L + 122, sy + 18, 92,
         24, IDC_EDIT_PLAY);
    hwnd_play_spin_ = mk(UPDOWN_CLASS, L"",
                         WS_CHILD | WS_VISIBLE | UDS_ALIGNRIGHT |
                             UDS_ARROWKEYS | UDS_SETBUDDYINT,
                         0, 0, 0, 0, IDC_SPIN_PLAY);
    SendMessageW(hwnd_play_spin_, UDM_SETBUDDY,
                 reinterpret_cast<WPARAM>(control(IDC_EDIT_PLAY)), 0);
    SendMessageW(hwnd_play_spin_, UDM_SETRANGE32, kPlaySliderMinDb,
                 kSliderMaxDb);
    mkLabel(L"dB", L + 224, sy + 22, 40, 20, 0);

    mkLabel(L"Silence thr (dB):", L + 10, sy + 52, 100, 20, 0);
    mkEx(WS_EX_CLIENTEDGE, L"EDIT", L"-55",
         WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_RIGHT, L + 122, sy + 48, 92,
         24, IDC_EDIT_SIL);
    hwnd_sil_spin_ = mk(UPDOWN_CLASS, L"",
                        WS_CHILD | WS_VISIBLE | UDS_ALIGNRIGHT |
                            UDS_ARROWKEYS | UDS_SETBUDDYINT,
                        0, 0, 0, 0, IDC_SPIN_SIL);
    SendMessageW(hwnd_sil_spin_, UDM_SETBUDDY,
                 reinterpret_cast<WPARAM>(control(IDC_EDIT_SIL)), 0);
    SendMessageW(hwnd_sil_spin_, UDM_SETRANGE32, kSliderMinDb, kSliderMaxDb);
    mkLabel(L"dB", L + 224, sy + 52, 40, 20, 0);

    mkLabel(L"Arm silence (min):", L + 10, sy + 82, 108, 20, 0);
    mkEx(WS_EX_CLIENTEDGE, L"EDIT", L"10",
         WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_RIGHT,
         L + 122, sy + 78, 92, 24, IDC_EDIT_ARM);
    hwnd_arm_spin_ = mk(UPDOWN_CLASS, L"",
                        WS_CHILD | WS_VISIBLE | UDS_ALIGNRIGHT |
                            UDS_ARROWKEYS | UDS_SETBUDDYINT,
                        0, 0, 0, 0, IDC_SPIN_ARM);
    SendMessageW(hwnd_arm_spin_, UDM_SETBUDDY,
                 reinterpret_cast<WPARAM>(control(IDC_EDIT_ARM)), 0);
    SendMessageW(hwnd_arm_spin_, UDM_SETRANGE32, 1, kArmMaxMin);
    mkLabel(L"minutes", L + 224, sy + 82, 70, 20, 0);

    mkLabel(L"Cooldown (sec):", L + 10, sy + 112, 104, 20, 0);
    mkEx(WS_EX_CLIENTEDGE, L"EDIT", L"5",
         WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_RIGHT,
         L + 122, sy + 108, 92, 24, IDC_EDIT_COOLDOWN);
    hwnd_cooldown_spin_ = mk(UPDOWN_CLASS, L"",
                             WS_CHILD | WS_VISIBLE | UDS_ALIGNRIGHT |
                                 UDS_ARROWKEYS | UDS_SETBUDDYINT,
                             0, 0, 0, 0, IDC_SPIN_COOLDOWN);
    SendMessageW(hwnd_cooldown_spin_, UDM_SETBUDDY,
                 reinterpret_cast<WPARAM>(control(IDC_EDIT_COOLDOWN)), 0);
    SendMessageW(hwnd_cooldown_spin_, UDM_SETRANGE32, 1, kCooldownMaxSec);
    mkLabel(L"seconds", L + 224, sy + 112, 70, 20, 0);

    hwnd_outputs_label_ =
        mkLabel(L"Outputs:", L + 10, sy + 142, 112, 20, IDC_STATIC_OUTPUTS);
    constexpr int output_ids[] = {IDC_CHK_OUTPUT_A1, IDC_CHK_OUTPUT_A2,
                                  IDC_CHK_OUTPUT_A3, IDC_CHK_OUTPUT_A4,
                                  IDC_CHK_OUTPUT_A5};
    constexpr const wchar_t* output_names[] = {L"A1", L"A2", L"A3", L"A4",
                                                L"A5"};
    for (int i = 0; i < 5; ++i) {
        const DWORD group_style = i == 0 ? WS_GROUP : 0;
        mk(L"BUTTON", output_names[i],
           WS_CHILD | WS_TABSTOP | group_style | BS_AUTORADIOBUTTON,
           L + 122 + i * 58, sy + 138, 54, 24, output_ids[i]);
    }

    mk(L"BUTTON", L"Run at startup", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_GROUP |
           BS_AUTOCHECKBOX, L + 10, sy + 172, 170, 22, IDC_CHK_AUTOSTART);
    mk(L"BUTTON", L"Start minimized to tray", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
           BS_AUTOCHECKBOX, L + 190, sy + 172, 225, 22, IDC_CHK_MINIMIZE);
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

void MainWindow::apply_settings_to_controls() {
    update_pause_button();
    Button_SetCheck(control(IDC_CHK_MINIMIZE),
                    settings_.start_minimized ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(control(IDC_CHK_AUTOSTART),
                    settings::get_autostart() ? BST_CHECKED : BST_UNCHECKED);

    wchar_t buf[48];

    int db = std::clamp(static_cast<int>(std::lround(settings_.play_threshold_db)),
                         kPlaySliderMinDb, kSliderMaxDb);
    SendMessageW(hwnd_play_spin_, UDM_SETPOS32, 0, db);

    db = std::clamp(static_cast<int>(std::lround(settings_.silence_threshold_db)),
                    kSliderMinDb, kSliderMaxDb);
    SendMessageW(hwnd_sil_spin_, UDM_SETPOS32, 0, db);

    const int min = std::clamp(
        static_cast<int>(std::lround(settings_.arm_after_ms / 60000.0)), 1,
        kArmMaxMin);
    SendMessageW(hwnd_arm_spin_, UDM_SETPOS32, 0, min);

    const int sec = std::clamp(
        static_cast<int>(std::lround(settings_.cooldown_ms / 1000.0)), 1,
        kCooldownMaxSec);
    SendMessageW(hwnd_cooldown_spin_, UDM_SETPOS32, 0, sec);

    constexpr int output_ids[] = {IDC_CHK_OUTPUT_A1, IDC_CHK_OUTPUT_A2,
                                  IDC_CHK_OUTPUT_A3, IDC_CHK_OUTPUT_A4,
                                  IDC_CHK_OUTPUT_A5};
    for (int i = 0; i < 5; ++i) {
        Button_SetCheck(control(output_ids[i]),
                        (settings_.output_bus_mask & (1u << i)) ? BST_CHECKED
                                                                : BST_UNCHECKED);
    }
    update_output_controls();

    swprintf(buf, 48, L"%d", settings_.restart_count);
    SetWindowTextW(hwnd_restart_count_, buf);
    SetWindowTextW(hwnd_last_restart_,
                   settings_.last_restart.empty() ? L"Never"
                                                  : settings_.last_restart.c_str());
}

void MainWindow::apply_controls_to_settings() {
    settings_.start_minimized =
        Button_GetCheck(control(IDC_CHK_MINIMIZE)) == BST_CHECKED;

    BOOL translated = FALSE;
    int pp = static_cast<int>(GetDlgItemInt(hwnd_, IDC_EDIT_PLAY, &translated, TRUE));
    if (!translated) pp = -50;
    pp = std::clamp(pp, kPlaySliderMinDb, kSliderMaxDb);
    settings_.play_threshold_db = static_cast<float>(pp);
    SendMessageW(hwnd_play_spin_, UDM_SETPOS32, 0, pp);

    translated = FALSE;
    int sp = static_cast<int>(GetDlgItemInt(hwnd_, IDC_EDIT_SIL, &translated, TRUE));
    if (!translated) sp = -55;
    sp = std::clamp(sp, kSliderMinDb, kSliderMaxDb);
    settings_.silence_threshold_db = static_cast<float>(sp);

    // Keep a hysteresis gap: silence must stay below the play threshold.
    if (settings_.silence_threshold_db >= settings_.play_threshold_db) {
        settings_.silence_threshold_db = settings_.play_threshold_db - 5.0f;
        sp = static_cast<int>(std::lround(settings_.silence_threshold_db));
        SendMessageW(hwnd_sil_spin_, UDM_SETPOS32, 0, sp);
    }

    translated = FALSE;
    int arm_min = static_cast<int>(
        GetDlgItemInt(hwnd_, IDC_EDIT_ARM, &translated, FALSE));
    if (!translated) arm_min = 1;
    arm_min = std::clamp(arm_min, 1, kArmMaxMin);
    settings_.arm_after_ms = arm_min * 60000;
    SendMessageW(hwnd_arm_spin_, UDM_SETPOS32, 0, arm_min);

    translated = FALSE;
    int cool_sec = static_cast<int>(
        GetDlgItemInt(hwnd_, IDC_EDIT_COOLDOWN, &translated, FALSE));
    if (!translated) cool_sec = 1;
    cool_sec = std::clamp(cool_sec, 1, kCooldownMaxSec);
    settings_.cooldown_ms = cool_sec * 1000;
    SendMessageW(hwnd_cooldown_spin_, UDM_SETPOS32, 0, cool_sec);

    const int count = output_count();
    if (count > 0) {
        const std::uint32_t valid_mask = (1u << count) - 1u;
        std::uint32_t mask = 0;
        constexpr int output_ids[] = {IDC_CHK_OUTPUT_A1, IDC_CHK_OUTPUT_A2,
                                      IDC_CHK_OUTPUT_A3, IDC_CHK_OUTPUT_A4,
                                      IDC_CHK_OUTPUT_A5};
        for (int i = 0; i < count; ++i) {
            if (Button_GetCheck(control(output_ids[i])) == BST_CHECKED) {
                mask |= 1u << i;
                break;
            }
        }
        settings_.output_bus_mask = mask ? mask & valid_mask : 0x01u;
    }
    update_output_controls();
}

void MainWindow::update_pause_button() {
    SetWindowTextW(control(IDC_BTN_TOGGLE),
                   settings_.enabled ? L"Pause" : L"Resume");
}

int MainWindow::output_count() const {
    if (!type_known_) return 0;
    switch (vm_type_) {
    case VoicemeeterRemote::Type::Voicemeeter:
        return 1; // Standard exposes A1/A2 as one shared 8-channel output.
    case VoicemeeterRemote::Type::Banana:
        return 3;
    case VoicemeeterRemote::Type::Potato:
    case VoicemeeterRemote::Type::PotatoX64:
        return 5;
    default:
        return 0;
    }
}

std::uint32_t MainWindow::selected_output_mask() const {
    const int count = output_count();
    if (count == 0) return 0;
    const std::uint32_t valid_mask = (1u << count) - 1u;
    const std::uint32_t selected = settings_.output_bus_mask & valid_mask;
    return selected ? selected & (~selected + 1u) : 0x01u;
}

std::wstring MainWindow::selected_outputs_text() const {
    if (vm_type_ == VoicemeeterRemote::Type::Voicemeeter) return L"A1/A2";

    const std::uint32_t mask = selected_output_mask();
    std::wstring text;
    for (int i = 0; i < output_count(); ++i) {
        if ((mask & (1u << i)) == 0) continue;
        if (!text.empty()) text += L", ";
        text += L"A" + std::to_wstring(i + 1);
    }
    return text;
}

void MainWindow::update_output_controls() {
    constexpr int output_ids[] = {IDC_CHK_OUTPUT_A1, IDC_CHK_OUTPUT_A2,
                                  IDC_CHK_OUTPUT_A3, IDC_CHK_OUTPUT_A4,
                                  IDC_CHK_OUTPUT_A5};
    const int count = output_count();
    SetWindowTextW(hwnd_outputs_label_,
                   count > 0 ? L"Monitor outputs:" : L"Outputs: connecting...");
    SetWindowTextW(control(IDC_CHK_OUTPUT_A1),
                   vm_type_ == VoicemeeterRemote::Type::Voicemeeter ? L"A1/A2"
                                                                    : L"A1");
    for (int i = 0; i < 5; ++i) {
        HWND checkbox = control(output_ids[i]);
        ShowWindow(checkbox, i < count ? SW_SHOW : SW_HIDE);
        EnableWindow(checkbox, i < count);
        if (i < count) {
            Button_SetCheck(checkbox,
                            (selected_output_mask() & (1u << i)) ? BST_CHECKED
                                                                  : BST_UNCHECKED);
        }
    }
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
            swprintf(buf, 64, L"Connected: Voicemeeter Standard");
            break;
        case VoicemeeterRemote::Type::Potato:
        case VoicemeeterRemote::Type::PotatoX64:
            swprintf(buf, 64, L"Connected: Voicemeeter Potato");
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
