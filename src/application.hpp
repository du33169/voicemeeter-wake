#pragma once

#include <windows.h>

namespace vmwake {

class MainWindow;

// Owns the Win32 application lifecycle: single-instance mutex, main window,
// message loop, and shutdown.
class Application {
public:
    Application() = default;
    ~Application();

    int run(HINSTANCE hInstance, int nCmdShow);

private:
    MainWindow* window_ = nullptr;
    HANDLE mutex_ = nullptr;
};

} // namespace vmwake
