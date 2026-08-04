#include "main_window.hpp"

#include <windowsx.h>

#include <cstdio>

#include "logger.hpp"
#include "win_util.hpp"

namespace vmwake {

void MainWindow::create_tray_icon() {
    nid_ = {};
    nid_.cbSize = sizeof(NOTIFYICONDATAW);
    nid_.hWnd = hwnd_;
    nid_.uID = 1;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = kTrayMsg;
    nid_.uVersion = NOTIFYICON_VERSION_4;
    nid_.hIcon = LoadIconW(hinstance_, MAKEINTRESOURCE(1));
    wcscpy(nid_.szTip, L"Voicemeeter Engine Wake");
    Shell_NotifyIconW(NIM_ADD, &nid_);
    Shell_NotifyIconW(NIM_SETVERSION, &nid_);
}

void MainWindow::remove_tray_icon() {
    if (nid_.hWnd) {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
        nid_ = {};
    }
}

void MainWindow::update_tray_tooltip() {
    if (!nid_.hWnd) return;
    wchar_t tip[128] = {0};
    swprintf(tip, 128, L"Voicemeeter Engine Wake%s",
             settings_.enabled ? L" - running" : L" - paused");
    wcscpy(nid_.szTip, tip);
    nid_.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
}

void MainWindow::on_tray(UINT msg) {
    switch (msg) {
    case WM_LBUTTONDBLCLK:
    case NIN_KEYSELECT:
        if (hidden_) {
            hidden_ = false;
            ShowWindow(hwnd_, SW_SHOW);
            SetForegroundWindow(hwnd_);
        }
        break;
    case WM_RBUTTONUP:
    case WM_CONTEXTMENU: {
        POINT pt = {};
        GetCursorPos(&pt);

        // The owner window must be the foreground window or the popup is
        // dismissed immediately.  A hidden (tray) window cannot take
        // foreground, so temporarily surface it without activation and
        // restore the hidden state after the menu closes.
        const bool was_hidden = hidden_ || !IsWindowVisible(hwnd_);
        if (was_hidden) {
            ShowWindow(hwnd_, SW_SHOWNA);
        }
        SetForegroundWindow(hwnd_);

        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, IDTRAY_SHOW, L"Show window");
        AppendMenuW(menu, MF_STRING, IDTRAY_TOGGLE,
                    settings_.enabled ? L"Pause auto-wake" : L"Resume auto-wake");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, IDTRAY_RESTART, L"Restart engine now");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, IDTRAY_EXIT, L"Exit");
        const int cmd =
            TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0,
                           hwnd_, nullptr);
        DestroyMenu(menu);
        // Let the shell reclaim foreground now that the menu is closed.
        PostMessageW(hwnd_, WM_NULL, 0, 0);

        if (was_hidden) {
            ShowWindow(hwnd_, SW_HIDE);
            hidden_ = true;
        }

        switch (cmd) {
        case IDTRAY_SHOW:
            hidden_ = false;
            ShowWindow(hwnd_, SW_SHOW);
            SetForegroundWindow(hwnd_);
            break;
        case IDTRAY_TOGGLE:
            settings_.enabled = !settings_.enabled;
            settings::save(settings_);
            Button_SetCheck(control(IDC_CHK_ENABLED),
                            settings_.enabled ? BST_CHECKED : BST_UNCHECKED);
            monitor_.reset();
            append_log(settings_.enabled ? L"Auto-wake: ENABLED"
                                         : L"Auto-wake: PAUSED");
            update_tray_tooltip();
            break;
        case IDTRAY_RESTART:
            append_log(L"Manual audio engine restart");
            do_restart();
            monitor_.reset();
            break;
        case IDTRAY_EXIT:
            Logger::instance().write(ftime() + L"  ===== exit =====");
            DestroyWindow(hwnd_);
            break;
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}

} // namespace vmwake
