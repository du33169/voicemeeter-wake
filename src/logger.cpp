#include "logger.hpp"

#include <algorithm>

namespace vmwake {

namespace {

constexpr size_t kMaxLogLines = 1000;

} // namespace

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::write(const std::wstring& line) {
    lines_.push_back(line);
    if (lines_.size() > kMaxLogLines) lines_.pop_front();
}

std::vector<std::wstring> Logger::tail(int max_lines) const {
    if (max_lines <= 0 || lines_.empty()) return {};

    const size_t count = std::min(lines_.size(), static_cast<size_t>(max_lines));
    return {lines_.end() - static_cast<std::ptrdiff_t>(count), lines_.end()};
}

} // namespace vmwake
