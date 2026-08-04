#pragma once

#include <windows.h>
#include <string>

namespace vmwake {

// Local timestamp "YYYY-MM-DD HH:MM:SS".
std::wstring ftime();

// Scale a design-unit value by the given DPI (96 = unscaled).
int dpi_scale(int value, unsigned int dpi);

} // namespace vmwake
