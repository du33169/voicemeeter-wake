#include "win_util.hpp"

#include <cstdio>

namespace vmwake {

std::wstring ftime() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[64] = {0};
    swprintf(buf, 64, L"%04u-%02u-%02u %02u:%02u:%02u", st.wYear, st.wMonth,
             st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

int dpi_scale(int value, unsigned int dpi) {
    return MulDiv(value, static_cast<int>(dpi), 96);
}

} // namespace vmwake
