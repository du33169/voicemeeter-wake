#include "application.hpp"

#include <dwmapi.h>

#include "app_info.hpp"
#include "logger.hpp"
#include "main_window.hpp"

namespace vmwake {

Application::~Application() {
    if (mutex_) {
        CloseHandle(mutex_);
        mutex_ = nullptr;
    }
}

int Application::run(HINSTANCE hInstance, int nCmdShow) {
    (void)nCmdShow; // showing/hiding is controlled by the start-minimized setting
    Logger::instance().init();

    mutex_ = CreateMutexW(nullptr, FALSE, appinfo::kMutexName);
    if (mutex_ && GetLastError() == ERROR_ALREADY_EXISTS) {
        // Already running: bring the existing window to the foreground and exit.
        HWND existing = FindWindowW(appinfo::kWindowClass, nullptr);
        if (existing) {
            if (IsIconic(existing)) ShowWindow(existing, SW_RESTORE);
            SetForegroundWindow(existing);
        }
        return 0;
    }

    MainWindow window;
    window_ = &window;
    if (!window.create(hInstance)) {
        window_ = nullptr;
        return 1;
    }
    window.run_message_loop();
    window_ = nullptr;
    return 0;
}

} // namespace vmwake
