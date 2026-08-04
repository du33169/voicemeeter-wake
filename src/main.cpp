#include <windows.h>

#include "application.hpp"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    // Best-effort DPI awareness; the manifest already requests PerMonitorV2.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    vmwake::Application app;
    return app.run(hInstance, nCmdShow);
}
