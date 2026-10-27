#pragma once
#include <string>

namespace pc {
// Shows an error message (UTF-8) and waits until it is closed. On Linux it
// is drawn with the system's fonts (the X11 message box of SDL has no
// Japanese characters), else SDL's message box; on Windows, the system's.
void showErrorBox(const std::string& message);
}
