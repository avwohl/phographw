#pragma once
// Dialog boxes for the Phograph Windows IDE.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>

// Show the fuzzy finder dialog. Returns the selected item name, or "" if cancelled.
std::string show_fuzzy_finder_dialog(HWND parent, HINSTANCE hInst,
                                      const std::vector<std::string>& items);
