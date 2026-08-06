#include "main_window.hpp"

#include <windowsx.h>
#include <commctrl.h>

#include <cstdio>

#include "app_info.hpp"
#include "logger.hpp"
#include "win_util.hpp"

namespace vmwake {

namespace {

// Fixed-size window: caption + system menu + minimize (no resize, no maximize).
constexpr DWORD kWindowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
                               WS_MINIMIZEBOX;

// Derive the monitor configuration from the persisted app settings.
MonitorConfig monitor_config_from(const AppSettings& s) {
    MonitorConfig cfg;
    cfg.sample_interval_ms = kTimerIntervalMs;
    cfg.play_threshold_db = s.play_threshold_db;
    cfg.silence_threshold_db = s.silence_threshold_db;
    cfg.confirm_samples = s.confirm_samples;
    cfg.arm_after_ms = s.arm_after_ms;
    cfg.cooldown_ms = s.cooldown_ms;
    return cfg;
}

} // namespace

MainWindow::~MainWindow() {
    // Window-side cleanup (timer, tray icon, remote logout/unload) already
    // happened in on_destroy(); only the GDI fonts are owned by the destructor.
    if (font_) {
        DeleteObject(font_);
        font_ = nullptr;
    }
    if (bold_font_) {
        DeleteObject(bold_font_);
        bold_font_ = nullptr;
    }
}

bool MainWindow::create(HINSTANCE hInstance) {
    hinstance_ = hInstance;
    taskbar_created_msg_ = RegisterWindowMessageW(L"TaskbarCreated");
    dpi_ = GetDpiForSystem();
    if (dpi_ == 0) dpi_ = 96;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = MainWindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCE(1));
    wc.hIconSm = wc.hIcon;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = appinfo::kWindowClass;
    if (!RegisterClassExW(&wc)) {
        Logger::instance().write(L"RegisterClassExW failed: " +
                                 std::to_wstring(GetLastError()));
        return false;
    }

    RECT window_rect = {0, 0, dpi_scale(584, dpi_), dpi_scale(770, dpi_)};
    AdjustWindowRectExForDpi(&window_rect, kWindowStyle, FALSE, 0, dpi_);
    const int cx = window_rect.right - window_rect.left;
    const int cy = window_rect.bottom - window_rect.top;

    // Load persisted settings before creating controls.
    settings_ = settings::load();
    monitor_cfg_ = monitor_config_from(settings_);
    monitor_ = AudioMonitor(monitor_cfg_);

    hwnd_ = CreateWindowExW(0, appinfo::kWindowClass, appinfo::kDisplayName,
                            kWindowStyle,
                            CW_USEDEFAULT, CW_USEDEFAULT, cx, cy, nullptr,
                            nullptr, hInstance, this);
    if (!hwnd_) {
        Logger::instance().write(L"CreateWindowExW failed: " +
                                 std::to_wstring(GetLastError()));
        return false;
    }

    const bool tray_available = create_tray_icon();
    Logger::instance().write(ftime() + L"  ===== startup =====  " +
                             (settings_.enabled ? L"auto-wake: ON"
                                                : L"auto-wake: OFF"));
    append_log(L"Program started (waiting for Voicemeeter)");

    if (settings_.start_minimized && tray_available) {
        hidden_ = true;
        ShowWindow(hwnd_, SW_HIDE);
    } else {
        ShowWindow(hwnd_, SW_SHOW);
        UpdateWindow(hwnd_);
    }
    return true;
}

void MainWindow::run_message_loop() {
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (IsDialogMessageW(hwnd_, &msg)) {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

HWND MainWindow::control(int id) const {
    return GetDlgItem(hwnd_, id);
}

void MainWindow::apply_font(HWND hwnd) {
    if (font_ && hwnd) {
        SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    }
}

LRESULT MainWindow::handle_message(UINT msg, WPARAM wParam, LPARAM lParam) {
    if (taskbar_created_msg_ != 0 && msg == taskbar_created_msg_) {
        create_tray_icon();
        return 0;
    }
    switch (msg) {
    case WM_CREATE:
        on_create();
        return 0;
    case WM_TIMER:
        on_timer();
        return 0;
    case WM_COMMAND:
        on_command(GET_WM_COMMAND_ID(wParam, lParam));
        return 0;
    case WM_NOTIFY:
        on_notify(lParam);
        return 0;
    case WM_DPICHANGED:
        on_dpi_changed(wParam, lParam);
        return 0;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        // Blend static text / group boxes / read-only edit with the white
        // client background instead of the default gray.
        SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
        SetTextColor(
            reinterpret_cast<HDC>(wParam),
            msg == WM_CTLCOLORSTATIC &&
                    GetDlgCtrlID(reinterpret_cast<HWND>(lParam)) ==
                        IDC_STATIC_VERSION
                ? GetSysColor(COLOR_GRAYTEXT)
                : GetSysColor(COLOR_WINDOWTEXT));
        return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
    case WM_ERASEBKGND: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        RECT rc;
        GetClientRect(hwnd_, &rc);
        FillRect(dc, &rc, static_cast<HBRUSH>(GetSysColorBrush(COLOR_WINDOW)));
        return 1;
    }
    case kTrayMsg:
        on_tray(wParam, lParam);
        return 0;
    case WM_CLOSE:
        on_close();
        return 0;
    case WM_DESTROY:
        on_destroy();
        return 0;
    default:
        return DefWindowProcW(hwnd_, msg, wParam, lParam);
    }
}

void MainWindow::on_create() {
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS | ICC_BAR_CLASSES |
                ICC_UPDOWN_CLASS | ICC_LINK_CLASS;
    InitCommonControlsEx(&icc);

    recreate_fonts();

    create_controls();
    apply_settings_to_controls();

    hwnd_log_ = control(IDC_EDIT_LOG);
    for (const auto& line : Logger::instance().tail(300)) {
        log_lines_.push_back(line);
    }
    if (log_lines_.empty()) log_lines_.push_back(ftime() + L"  log is empty");
    std::wstring all;
    for (const auto& l : log_lines_) {
        all += l;
        all += L"\r\n";
    }
    SetWindowTextW(hwnd_log_, all.c_str());
    const LRESULT end = GetWindowTextLengthW(hwnd_log_);
    SendMessageW(hwnd_log_, EM_SETSEL, end, end);
    SendMessageW(hwnd_log_, EM_SCROLLCARET, 0, 0);
    SendMessageW(hwnd_log_, WM_VSCROLL, SB_BOTTOM, 0);

    if (!start_timer()) {
        append_log(L"Error: failed to start monitor timer");
        set_state_text(L"Monitor unavailable");
    }
    refresh_status_texts();
}

void MainWindow::on_timer() {
    tick_++;
    poll_voicemeeter();
}

void MainWindow::on_command(int id) {
    switch (id) {
    case IDC_BTN_START_VM:
        launch_voicemeeter();
        break;
    case IDC_BTN_RESTART:
        restart_manually();
        break;
    case IDC_BTN_EXIT:
        exit_application();
        break;
    case IDC_BTN_TOGGLE: {
        toggle_auto_wake();
        break;
    }
    case IDC_BTN_SAVE:
        apply_controls_to_settings();
        if (settings::save(settings_)) {
            append_log(L"Settings saved");
        } else {
            append_log(L"Error: settings could not be persisted");
        }
        monitor_cfg_ = monitor_config_from(settings_);
        monitor_.set_config(monitor_cfg_);
        refresh_status_texts();
        break;
    case IDC_CHK_AUTOSTART:
        if (settings::set_autostart(
                Button_GetCheck(control(IDC_CHK_AUTOSTART)) == BST_CHECKED)) {
            append_log(Button_GetCheck(control(IDC_CHK_AUTOSTART)) == BST_CHECKED
                           ? L"Run at startup: ENABLED"
                           : L"Run at startup: DISABLED");
        } else {
            Button_SetCheck(control(IDC_CHK_AUTOSTART),
                            settings::get_autostart() ? BST_CHECKED : BST_UNCHECKED);
            append_log(L"Error: run-at-startup setting could not be changed");
        }
        break;
    case IDC_CHK_MINIMIZE:
        settings_.start_minimized =
            Button_GetCheck(control(IDC_CHK_MINIMIZE)) == BST_CHECKED;
        if (!settings::save(settings_)) {
            append_log(L"Error: start-minimized setting could not be persisted");
        }
        break;
    case IDC_CHK_NOTIFY_ON_CLOSE:
        settings_.notify_on_close =
            Button_GetCheck(control(IDC_CHK_NOTIFY_ON_CLOSE)) == BST_CHECKED;
        if (!settings::save(settings_)) {
            append_log(L"Error: close-notification setting could not be persisted");
        }
        break;
    default:
        break;
    }
}

void MainWindow::on_dpi_changed(WPARAM wParam, LPARAM lParam) {
    const UINT new_dpi = LOWORD(wParam);
    if (new_dpi == 0 || new_dpi == dpi_) return;

    const UINT old_dpi = dpi_;
    const auto* suggested = reinterpret_cast<const RECT*>(lParam);
    SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top,
                 suggested->right - suggested->left, suggested->bottom - suggested->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    for (HWND child = GetWindow(hwnd_, GW_CHILD); child;
         child = GetWindow(child, GW_HWNDNEXT)) {
        RECT rc;
        GetWindowRect(child, &rc);
        MapWindowPoints(nullptr, hwnd_, reinterpret_cast<POINT*>(&rc), 2);
        SetWindowPos(child, nullptr, MulDiv(rc.left, new_dpi, old_dpi),
                     MulDiv(rc.top, new_dpi, old_dpi),
                     MulDiv(rc.right - rc.left, new_dpi, old_dpi),
                     MulDiv(rc.bottom - rc.top, new_dpi, old_dpi),
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
    dpi_ = new_dpi;
    recreate_fonts();
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void MainWindow::toggle_auto_wake() {
    settings_.enabled = !settings_.enabled;
    if (!settings::save(settings_)) {
        append_log(L"Error: auto-wake state could not be persisted");
    }
    monitor_.reset();
    append_log(settings_.enabled ? L"Auto-wake: ENABLED" : L"Auto-wake: PAUSED");
    update_pause_button();
    update_tray_tooltip();
}

void MainWindow::restart_manually() {
    if (!can_restart()) {
        append_log(L"Manual restart unavailable: Voicemeeter is not connected");
        return;
    }
    append_log(L"Manual audio engine restart");
    if (do_restart()) monitor_.reset();
}

bool MainWindow::can_restart() const {
    return remote_.loaded() && logged_in_ && vm_running_ && type_known_;
}

void MainWindow::exit_application() {
    Logger::instance().write(ftime() + L"  ===== exit =====");
    DestroyWindow(hwnd_);
}

void MainWindow::launch_voicemeeter() {
    VoicemeeterRemote::Type type = vm_type_;
    if (!type_known_) {
        constexpr UINT kLaunchStandard = 2101;
        constexpr UINT kLaunchBanana = 2102;
        constexpr UINT kLaunchPotato = 2103;

        RECT rc;
        GetWindowRect(control(IDC_BTN_START_VM), &rc);
        HMENU menu = CreatePopupMenu();
        if (!menu) {
            append_log(L"Error: failed to open Voicemeeter version menu");
            return;
        }
        AppendMenuW(menu, MF_STRING, kLaunchStandard, L"Voicemeeter Standard");
        AppendMenuW(menu, MF_STRING, kLaunchBanana, L"Voicemeeter Banana");
        AppendMenuW(menu, MF_STRING, kLaunchPotato, L"Voicemeeter Potato");
        const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN,
                                             rc.left, rc.bottom, 0, hwnd_, nullptr);
        DestroyMenu(menu);
        if (command == 0) return;
        type = command == kLaunchStandard ? VoicemeeterRemote::Type::Voicemeeter
             : command == kLaunchBanana ? VoicemeeterRemote::Type::Banana
                                        : VoicemeeterRemote::Type::Potato;
    }
    if (type == VoicemeeterRemote::Type::PotatoX64) {
        type = VoicemeeterRemote::Type::Potato;
    }
    const wchar_t* name = type == VoicemeeterRemote::Type::Voicemeeter ? L"Standard"
                        : type == VoicemeeterRemote::Type::Banana ? L"Banana"
                                                                  : L"Potato";
    if (remote_.run_voicemeeter(type) == 0) {
        append_log(L"Launch request sent for Voicemeeter " + std::wstring(name));
    } else {
        append_log(L"Error: failed to launch Voicemeeter " + std::wstring(name));
    }
}

void MainWindow::recreate_fonts() {
    NONCLIENTMETRICSW ncm = {};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    ncm.lfMessageFont.lfHeight = -MulDiv(9, static_cast<int>(dpi_), 72);
    ncm.lfMessageFont.lfQuality = CLEARTYPE_QUALITY;
    LOGFONTW bold_lf = ncm.lfMessageFont;
    bold_lf.lfWeight = FW_BOLD;
    HFONT new_font = CreateFontIndirectW(&ncm.lfMessageFont);
    HFONT new_bold_font = CreateFontIndirectW(&bold_lf);
    if (!new_font || !new_bold_font) {
        if (new_font) DeleteObject(new_font);
        if (new_bold_font) DeleteObject(new_bold_font);
        return;
    }

    HFONT old_font = font_;
    HFONT old_bold_font = bold_font_;
    font_ = new_font;
    bold_font_ = new_bold_font;
    apply_font(hwnd_);
    for (HWND child = GetWindow(hwnd_, GW_CHILD); child;
         child = GetWindow(child, GW_HWNDNEXT)) {
        const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(child, GWL_STYLE));
        const HFONT child_font = (style & BS_TYPEMASK) == BS_GROUPBOX ? bold_font_ : font_;
        SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(child_font), TRUE);
    }
    if (old_font) DeleteObject(old_font);
    if (old_bold_font) DeleteObject(old_bold_font);
}

void MainWindow::on_close() {
    if (hidden_) return;
    hidden_ = true;
    ShowWindow(hwnd_, SW_HIDE);

    if (settings_.notify_on_close) {
        nid_.uFlags = NIF_INFO | NIF_TIP;
        wcscpy(nid_.szInfoTitle, appinfo::kDisplayName);
        wcscpy(nid_.szInfo,
               L"Still running in the background - auto-wake is active. "
               L"Right-click this icon to exit.");
        Shell_NotifyIconW(NIM_MODIFY, &nid_);
        nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    }
}

void MainWindow::on_destroy() {
    if (timer_id_) {
        KillTimer(hwnd_, timer_id_);
        timer_id_ = 0;
    }
    if (remote_.loaded()) {
        const long rc = remote_.logout();
        Logger::instance().write(ftime() + L"  Remote logout: " +
                                 std::to_wstring(rc));
        remote_.unload();
    }
    logged_in_ = false;
    type_known_ = false;
    remove_tray_icon();
    PostQuitMessage(0);
}

LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT msg, WPARAM wParam,
                                LPARAM lParam) {
    MainWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        const auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<MainWindow*>(cs->lpCreateParams);
        self->hwnd_ = hwnd; // make the handle available during WM_CREATE
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<MainWindow*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) {
        return self->handle_message(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace vmwake
