#include "main_window.hpp"

#include <windowsx.h>

#include <cstdio>

#include "app_info.hpp"
#include "logger.hpp"
#include "win_util.hpp"

namespace vmwake {

bool MainWindow::create_tray_icon() {
    if (tray_icon_added_) remove_tray_icon();
    nid_ = {};
    nid_.cbSize = sizeof(NOTIFYICONDATAW);
    nid_.hWnd = hwnd_;
    nid_.uID = 1;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = kTrayMsg;
    nid_.uVersion = NOTIFYICON_VERSION_4;
    nid_.hIcon = LoadIconW(hinstance_, MAKEINTRESOURCE(1));
    wcscpy(nid_.szTip, appinfo::kDisplayName);
    tray_icon_added_ = Shell_NotifyIconW(NIM_ADD, &nid_) != FALSE;
    if (!tray_icon_added_) {
        Logger::instance().write(ftime() + L"  Failed to create tray icon");
        nid_ = {};
        tray_uses_v4_ = false;
        return false;
    }
    tray_uses_v4_ = Shell_NotifyIconW(NIM_SETVERSION, &nid_) != FALSE;
    return true;
}

void MainWindow::remove_tray_icon() {
    if (tray_icon_added_) {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
    }
    nid_ = {};
    tray_icon_added_ = false;
    tray_uses_v4_ = false;
}

void MainWindow::update_tray_tooltip() {
    if (!tray_icon_added_) return;
    wchar_t tip[128] = {0};
    swprintf(tip, 128, L"%ls%ls", appinfo::kDisplayName,
             settings_.enabled ? L" - running" : L" - paused");
    wcscpy(nid_.szTip, tip);
    nid_.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
}

void MainWindow::on_tray(WPARAM wParam, LPARAM lParam) {
    const UINT msg = tray_uses_v4_ ? LOWORD(lParam)
                                   : static_cast<UINT>(lParam);
    switch (msg) {
    case WM_LBUTTONDBLCLK:
    case NIN_SELECT:
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
        if (tray_uses_v4_ && msg == WM_CONTEXTMENU) {
            pt.x = GET_X_LPARAM(wParam);
            pt.y = GET_Y_LPARAM(wParam);
        } else {
            GetCursorPos(&pt);
        }

        // A hidden top-level window can own the popup; it does not need to be
        // made visible. WM_NULL below lets the shell dismiss the menu cleanly.
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

        switch (cmd) {
        case IDTRAY_SHOW:
            hidden_ = false;
            ShowWindow(hwnd_, SW_SHOW);
            SetForegroundWindow(hwnd_);
            break;
        case IDTRAY_TOGGLE:
            toggle_auto_wake();
            break;
        case IDTRAY_RESTART:
            restart_manually();
            break;
        case IDTRAY_EXIT:
            exit_application();
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
