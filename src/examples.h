#pragma once
// Example browser: downloads examples from GitHub releases, caches locally,
// and provides a categorized browser dialog.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>

struct ExampleEntry {
    std::string name;
    std::string description;
    std::string file;
};

struct ExampleCategory {
    std::string name;
    std::vector<ExampleEntry> examples;
};

// Show the example browser. Downloads examples on first use, then shows
// a categorized browser dialog. Returns the selected example's JSON content,
// or "" if cancelled or on error.
std::string show_example_browser(HWND parent, HINSTANCE hInst);
