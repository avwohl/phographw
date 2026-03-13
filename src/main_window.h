#pragma once
// Main IDE window: manages layout, menus, toolbar, and child panels.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <memory>

#include "app.h"
#include "graph_canvas.h"

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool create(HINSTANCE hInst);
    HWND hwnd() const { return hwnd_; }
    PhographApp& app() { return app_; }

    // Command handlers
    void on_file_new();
    void on_file_open();
    void on_file_save();
    void on_file_save_as();
    void on_run();
    void on_debug();
    void on_stop();
    void on_debug_continue();
    void on_debug_step_over();
    void on_debug_step_into();
    void on_add_node();
    void on_delete_selected();
    void on_select_all();
    void on_duplicate();
    void on_toggle_sidebar();
    void on_toggle_inspector();
    void on_toggle_console();
    void on_about();
    void on_docs();

    // Refresh UI state
    void update_ui();
    void update_console();
    void update_sidebar();
    void update_status();

    // Static window procedure
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

private:
    void create_controls();
    void layout_controls();
    void populate_sidebar();

    // Fuzzy finder dialog
    void show_fuzzy_finder();

    HWND hwnd_ = nullptr;
    HINSTANCE hInst_ = nullptr;

    // Child controls
    HWND sidebar_ = nullptr;      // TreeView
    HWND console_ = nullptr;      // Edit (multiline)
    HWND statusbar_ = nullptr;    // StatusBar
    HWND inspector_ = nullptr;    // Static + child edits
    HWND toolbar_ = nullptr;      // Toolbar
    HWND splitter_h_ = nullptr;   // Horizontal splitter (for console)

    std::unique_ptr<GraphCanvas> canvas_;

    PhographApp app_;

    // Panel visibility
    bool show_sidebar_ = true;
    bool show_inspector_ = true;
    bool show_console_ = true;

    // Layout sizes
    int sidebar_width_ = 200;
    int inspector_width_ = 200;
    int console_height_ = 100;
};
