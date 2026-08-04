#include "logger.hpp"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <deque>
#include <filesystem>

namespace vmwake {

namespace {

constexpr unsigned long long kMaxLogBytes = 512ull * 1024ull;

std::wstring local_app_data_dir() {
    wchar_t buf[MAX_PATH] = {0};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr,
                                SHGFP_TYPE_CURRENT, buf))) {
        return {};
    }
    return buf;
}

// Convert a byte buffer (log file is UTF-8) to a wide string.
std::wstring utf8_to_wide(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(),
                                      static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring out(static_cast<size_t>(n), 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
                        &out[0], n);
    return out;
}

} // namespace

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::~Logger() {
    if (file_ != INVALID_HANDLE_VALUE) CloseHandle(file_);
}

void Logger::init() {
    const std::wstring base = local_app_data_dir();
    if (base.empty()) return;
    const std::wstring dir = base + L"\\VoicemeeterEngineWake";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        OutputDebugStringW((L"Failed to create log directory: " +
                            std::to_wstring(ec.value()) + L"\n").c_str());
        return;
    }
    path_ = dir + L"\\app.log";
    ensure_file();
}

bool Logger::ensure_file() {
    if (file_ != INVALID_HANDLE_VALUE) return true;
    if (path_.empty()) return false;

    rotate_if_needed(0);

    file_ = CreateFileW(path_.c_str(), FILE_APPEND_DATA,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file_ != INVALID_HANDLE_VALUE) {
        SetFilePointer(file_, 0, nullptr, FILE_END);
    }
    return file_ != INVALID_HANDLE_VALUE;
}

bool Logger::rotate_if_needed(size_t additional_bytes) {
    if (path_.empty()) return false;

    WIN32_FILE_ATTRIBUTE_DATA info{};
    if (!GetFileAttributesExW(path_.c_str(), GetFileExInfoStandard, &info)) {
        return true;
    }
    const unsigned long long size =
        (static_cast<unsigned long long>(info.nFileSizeHigh) << 32) |
        info.nFileSizeLow;
    if (size + additional_bytes <= kMaxLogBytes) return true;

    if (file_ != INVALID_HANDLE_VALUE) {
        CloseHandle(file_);
        file_ = INVALID_HANDLE_VALUE;
    }
    const std::wstring old = path_ + L".1";
    DeleteFileW(old.c_str());
    if (!MoveFileExW(path_.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        OutputDebugStringW(L"Failed to rotate log file; continuing append\n");
    }
    return true;
}

void Logger::write(const std::wstring& line) {
    int n = WideCharToMultiByte(CP_UTF8, 0, line.c_str(),
                                static_cast<int>(line.size()), nullptr, 0,
                                nullptr, nullptr);
    std::string utf8;
    if (n > 0) {
        utf8.resize(static_cast<size_t>(n));
        WideCharToMultiByte(CP_UTF8, 0, line.c_str(),
                            static_cast<int>(line.size()), &utf8[0], n, nullptr,
                            nullptr);
    }
    std::string out = utf8 + "\r\n";
    if (!rotate_if_needed(out.size()) || !ensure_file()) return;
    DWORD written = 0;
    if (!WriteFile(file_, out.data(), static_cast<DWORD>(out.size()), &written,
                   nullptr) || written != out.size()) {
        OutputDebugStringW(L"Failed to write complete log line\n");
    }
}

std::vector<std::wstring> Logger::tail(int max_lines) const {
    if (file_ == INVALID_HANDLE_VALUE || path_.empty() || max_lines <= 0) return {};

    WIN32_FILE_ATTRIBUTE_DATA info{};
    if (!GetFileAttributesExW(path_.c_str(), GetFileExInfoStandard, &info)) {
        return {};
    }
    if (info.nFileSizeHigh != 0) return {};
    const size_t size = info.nFileSizeLow;
    if (size == 0) return {};

    std::string raw(size, 0);
    HANDLE f = CreateFileW(path_.c_str(), GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return {};
    DWORD read = 0;
    ReadFile(f, &raw[0], static_cast<DWORD>(size), &read, nullptr);
    CloseHandle(f);
    raw.resize(read);

    const std::wstring text = utf8_to_wide(raw);

    std::deque<std::wstring> lines;
    std::wstring cur;
    for (const wchar_t ch : text) {
        if (ch == L'\n') {
            if (!cur.empty()) lines.push_back(cur);
            cur.clear();
        } else if (ch != L'\r') {
            cur.push_back(ch);
        }
    }
    if (!cur.empty()) lines.push_back(cur);

    std::vector<std::wstring> out;
    const size_t start = lines.size() > static_cast<size_t>(max_lines)
                             ? lines.size() - static_cast<size_t>(max_lines)
                             : 0;
    for (size_t i = start; i < lines.size(); ++i) {
        out.push_back(lines[i]);
    }
    return out;
}

} // namespace vmwake
