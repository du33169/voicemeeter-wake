#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace vmwake {

// Minimal file-backed logger with in-memory tail for the GUI log pane.
class Logger {
public:
    static Logger& instance();

    // Create/open the log file under %LOCALAPPDATA%\VoicemeeterEngineWake
    // (rotating a single .1 file when the log exceeds the size limit).
    void init();

    // Append a line (no timestamp added here; callers may add their own).
    void write(const std::wstring& line);

    std::vector<std::wstring> tail(int max_lines) const;

private:
    Logger() = default;
    bool ensure_file();

    HANDLE file_ = INVALID_HANDLE_VALUE;
    std::wstring path_;
};

} // namespace vmwake
