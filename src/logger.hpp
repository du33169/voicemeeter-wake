#pragma once

#include <deque>
#include <string>
#include <vector>

namespace vmwake {

// Process-local logger backing the GUI log pane.
class Logger {
public:
    static Logger& instance();

    void init() {}

    // Append a line (no timestamp added here; callers may add their own).
    void write(const std::wstring& line);

    std::vector<std::wstring> tail(int max_lines) const;

private:
    Logger() = default;

    std::deque<std::wstring> lines_;
};

} // namespace vmwake
