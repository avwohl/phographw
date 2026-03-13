#include "main_window.h"
#include "resource.h"
#include "dialogs.h"
#include "examples.h"
#include <windowsx.h>
#include <commdlg.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <vector>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comdlg32.lib")

static const wchar_t* MAIN_CLASS = L"PhographMainWindow";

MainWindow::MainWindow() {}
MainWindow::~MainWindow() {}

bool MainWindow::create(HINSTANCE hInst) {
    hInst_ = hInst;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWindow::WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
    wc.lpszMenuName = L"MainMenu";
    wc.lpszClassName = MAIN_CLASS;
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_PHOGRAPH));
    wc.hIconSm = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_PHOGRAPH),
        IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(
        WS_EX_APPWINDOW,
        MAIN_CLASS, L"Phograph - Visual Dataflow Programming",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800,
        nullptr, nullptr, hInst, this);

    if (!hwnd_) return false;

    create_controls();
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    // Start with a new project
    app_.new_project();
    update_ui();

    return true;
}

void MainWindow::create_controls() {
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_TREEVIEW_CLASSES | ICC_BAR_CLASSES | ICC_COOL_CLASSES;
    InitCommonControlsEx(&icex);

    RECT rc;
    GetClientRect(hwnd_, &rc);

    // Toolbar
    toolbar_ = CreateWindowExW(0, TOOLBARCLASSNAMEW, nullptr,
        WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | CCS_NODIVIDER,
        0, 0, 0, 0, hwnd_, (HMENU)IDC_TOOLBAR, hInst_, nullptr);
    SendMessage(toolbar_, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);

    // Add toolbar buttons
    TBBUTTON buttons[] = {
        {0, IDM_RUN_RUN,     TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Run"},
        {0, IDM_RUN_DEBUG,   TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Debug"},
        {0, IDM_RUN_STOP,    0,               BTNS_BUTTON, {0}, 0, (INT_PTR)L"Stop"},
        {0, 0,               TBSTATE_ENABLED, BTNS_SEP,    {0}, 0, 0},
        {0, IDM_NODE_ADD,    TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Add Node"},
    };
    SendMessage(toolbar_, TB_SETMAXTEXTROWS, 1, 0);
    SendMessage(toolbar_, TB_ADDBUTTONS, _countof(buttons), (LPARAM)buttons);
    SendMessage(toolbar_, TB_AUTOSIZE, 0, 0);

    // Sidebar (TreeView)
    sidebar_ = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, nullptr,
        WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
        0, 0, sidebar_width_, 400,
        hwnd_, (HMENU)IDC_SIDEBAR, hInst_, nullptr);

    // Canvas
    canvas_ = std::make_unique<GraphCanvas>();
    canvas_->create(hwnd_, sidebar_width_, 28, 600, 400, hInst_);
    canvas_->set_app(&app_);

    // Inspector (simple static window for now)
    inspector_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"Inspector",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, inspector_width_, 400,
        hwnd_, (HMENU)IDC_INSPECTOR, hInst_, nullptr);

    // Console (multiline edit)
    console_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        0, 0, 600, console_height_,
        hwnd_, (HMENU)IDC_CONSOLE, hInst_, nullptr);

    // Set console font to monospace
    HFONT mono_font = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    SendMessage(console_, WM_SETFONT, (WPARAM)mono_font, TRUE);

    // Status bar
    statusbar_ = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, hwnd_, (HMENU)IDC_STATUSBAR, hInst_, nullptr);

    layout_controls();
}

void MainWindow::layout_controls() {
    RECT rc;
    GetClientRect(hwnd_, &rc);
    int w = rc.right, h = rc.bottom;

    // Toolbar height
    RECT tbrc;
    GetWindowRect(toolbar_, &tbrc);
    int toolbar_h = tbrc.bottom - tbrc.top;
    SendMessage(toolbar_, TB_AUTOSIZE, 0, 0);

    // Status bar
    SendMessage(statusbar_, WM_SIZE, 0, 0);
    RECT sbrc;
    GetWindowRect(statusbar_, &sbrc);
    int status_h = sbrc.bottom - sbrc.top;

    int top = toolbar_h;
    int bottom = h - status_h;
    int content_h = bottom - top;

    // Console at bottom
    int con_h = show_console_ ? console_height_ : 0;
    int canvas_h = content_h - con_h;

    // Sidebar on left
    int sb_w = show_sidebar_ ? sidebar_width_ : 0;

    // Inspector on right
    int insp_w = show_inspector_ ? inspector_width_ : 0;

    // Canvas in the middle
    int canvas_w = w - sb_w - insp_w;
    if (canvas_w < 100) canvas_w = 100;

    MoveWindow(sidebar_, 0, top, sb_w, canvas_h, TRUE);
    ShowWindow(sidebar_, show_sidebar_ ? SW_SHOW : SW_HIDE);

    if (canvas_) {
        MoveWindow(canvas_->hwnd(), sb_w, top, canvas_w, canvas_h, TRUE);
    }

    MoveWindow(inspector_, sb_w + canvas_w, top, insp_w, canvas_h, TRUE);
    ShowWindow(inspector_, show_inspector_ ? SW_SHOW : SW_HIDE);

    MoveWindow(console_, 0, top + canvas_h, w, con_h, TRUE);
    ShowWindow(console_, show_console_ ? SW_SHOW : SW_HIDE);
}

void MainWindow::populate_sidebar() {
    TreeView_DeleteAllItems(sidebar_);

    for (auto& sec : app_.sections()) {
        TVINSERTSTRUCTW tvis = {};
        tvis.hParent = TVI_ROOT;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT;
        std::wstring wsec(sec.name.begin(), sec.name.end());
        tvis.item.pszText = (LPWSTR)wsec.c_str();
        HTREEITEM hSection = TreeView_InsertItem(sidebar_, &tvis);

        // Add methods
        for (auto& m : sec.methods) {
            TVINSERTSTRUCTW mvis = {};
            mvis.hParent = hSection;
            mvis.hInsertAfter = TVI_LAST;
            mvis.item.mask = TVIF_TEXT | TVIF_PARAM;
            std::wstring wm(m.name.begin(), m.name.end());
            // Add pin counts to display
            std::wstring display = wm + L" (" + std::to_wstring(m.num_inputs) +
                                   L" in, " + std::to_wstring(m.num_outputs) + L" out)";
            mvis.item.pszText = (LPWSTR)display.c_str();
            mvis.item.lParam = 0;
            TreeView_InsertItem(sidebar_, &mvis);
        }

        // Add classes
        for (auto& cls : sec.classes) {
            TVINSERTSTRUCTW cvis = {};
            cvis.hParent = hSection;
            cvis.hInsertAfter = TVI_LAST;
            cvis.item.mask = TVIF_TEXT;
            std::wstring wc(cls.name.begin(), cls.name.end());
            std::wstring cdisplay = L"[Class] " + wc;
            cvis.item.pszText = (LPWSTR)cdisplay.c_str();
            HTREEITEM hClass = TreeView_InsertItem(sidebar_, &cvis);

            // Class attributes
            for (auto& attr : cls.attributes) {
                TVINSERTSTRUCTW avis = {};
                avis.hParent = hClass;
                avis.hInsertAfter = TVI_LAST;
                avis.item.mask = TVIF_TEXT;
                std::wstring wa(attr.begin(), attr.end());
                std::wstring adisplay = L"@ " + wa;
                avis.item.pszText = (LPWSTR)adisplay.c_str();
                TreeView_InsertItem(sidebar_, &avis);
            }

            // Class methods
            for (auto& m : cls.methods) {
                TVINSERTSTRUCTW mvis = {};
                mvis.hParent = hClass;
                mvis.hInsertAfter = TVI_LAST;
                mvis.item.mask = TVIF_TEXT;
                std::wstring wm(m.name.begin(), m.name.end());
                std::wstring mdisplay = wm + L" (" + std::to_wstring(m.num_inputs) +
                                        L" in, " + std::to_wstring(m.num_outputs) + L" out)";
                mvis.item.pszText = (LPWSTR)mdisplay.c_str();
                TreeView_InsertItem(sidebar_, &mvis);
            }
        }

        TreeView_Expand(sidebar_, hSection, TVE_EXPAND);
    }
}

// -----------------------------------------------------------------------
// Command handlers
// -----------------------------------------------------------------------

void MainWindow::on_file_new() {
    app_.new_project();
    update_ui();
}

void MainWindow::on_file_open() {
    wchar_t filename[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFilter = L"Phograph Projects (*.json;*.phograph.json)\0*.json;*.phograph.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Open Phograph Project";

    if (GetOpenFileNameW(&ofn)) {
        app_.load_project_file(filename);
        update_ui();
    }
}

void MainWindow::on_file_save() {
    if (app_.get_project_json().empty()) return;
    // If we have a path, save there; otherwise Save As
    on_file_save_as();
}

void MainWindow::on_file_save_as() {
    wchar_t filename[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFilter = L"Phograph Projects (*.phograph.json)\0*.phograph.json\0JSON Files (*.json)\0*.json\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = L"json";
    ofn.lpstrTitle = L"Save Phograph Project";

    if (GetSaveFileNameW(&ofn)) {
        app_.save_project_file(filename);
        update_status();
    }
}

void MainWindow::on_run() {
    if (app_.selected_method().empty()) {
        app_.append_console("No method selected\n");
        update_console();
        return;
    }
    app_.run_method(app_.selected_method());
    update_console();
    update_status();
}

void MainWindow::on_debug() {
    if (app_.selected_method().empty()) return;
    app_.debug_run(app_.selected_method());
    update_console();
    update_status();
}

void MainWindow::on_stop() {
    app_.stop_execution();
    update_status();
}

void MainWindow::on_debug_continue() { app_.debug_continue(); }
void MainWindow::on_debug_step_over() { app_.debug_step_over(); }
void MainWindow::on_debug_step_into() { app_.debug_step_into(); }

void MainWindow::on_add_node() {
    show_fuzzy_finder();
}

void MainWindow::on_delete_selected() {
    // TODO: implement delete selected nodes/wires
}

void MainWindow::on_select_all() {
    if (app_.current_graph()) {
        for (auto& n : app_.current_graph()->nodes) n.selected = true;
        if (canvas_) canvas_->invalidate();
    }
}

void MainWindow::on_duplicate() {
    // TODO: implement duplicate
}

void MainWindow::on_toggle_sidebar() {
    show_sidebar_ = !show_sidebar_;
    layout_controls();
}

void MainWindow::on_toggle_inspector() {
    show_inspector_ = !show_inspector_;
    layout_controls();
}

void MainWindow::on_toggle_console() {
    show_console_ = !show_console_;
    layout_controls();
}

// -----------------------------------------------------------------------
// About dialog (matches macOS about box with clickable source link)
// -----------------------------------------------------------------------

static INT_PTR CALLBACK AboutDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right;

        HFONT titleFont = CreateFontW(22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

        HFONT bodyFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

        int y = 14;

        // App name
        HWND hTitle = CreateWindowExW(0, L"STATIC", L"Phograph",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            0, y, w, 26, hwnd, nullptr, nullptr, nullptr);
        SendMessage(hTitle, WM_SETFONT, (WPARAM)titleFont, TRUE);
        y += 30;

        // Version
        HWND hVer = CreateWindowExW(0, L"STATIC", L"Version 0.1.0",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            0, y, w, 16, hwnd, nullptr, nullptr, nullptr);
        SendMessage(hVer, WM_SETFONT, (WPARAM)bodyFont, TRUE);
        y += 26;

        // Description
        HWND hDesc = CreateWindowExW(0, L"STATIC",
            L"A modern implementation of the Prograph\n"
            L"visual dataflow programming language.",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            10, y, w - 20, 36, hwnd, nullptr, nullptr, nullptr);
        SendMessage(hDesc, WM_SETFONT, (WPARAM)bodyFont, TRUE);
        y += 42;

        // Author
        HWND hAuthor = CreateWindowExW(0, L"STATIC", L"Author: Aaron Wohl",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            0, y, w, 16, hwnd, nullptr, nullptr, nullptr);
        SendMessage(hAuthor, WM_SETFONT, (WPARAM)bodyFont, TRUE);
        y += 18;

        // Copyright
        HWND hCopy = CreateWindowExW(0, L"STATIC",
            L"Copyright \x00A9 2025-2026 Aaron Wohl.\nAll rights reserved.",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            0, y, w, 32, hwnd, nullptr, nullptr, nullptr);
        SendMessage(hCopy, WM_SETFONT, (WPARAM)bodyFont, TRUE);
        y += 38;

        // Clickable source link (SysLink control)
        HWND hLink = CreateWindowExW(0, WC_LINK,
            L"<a href=\"https://github.com/avwohl/phographw\">https://github.com/avwohl/phographw</a>",
            WS_CHILD | WS_VISIBLE,
            24, y, w - 48, 16, hwnd, (HMENU)(INT_PTR)IDC_ABOUT_LINK, nullptr, nullptr);
        SendMessage(hLink, WM_SETFONT, (WPARAM)bodyFont, TRUE);
        y += 28;

        // OK button
        HWND hOK = CreateWindowExW(0, L"BUTTON", L"OK",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            (w - 80) / 2, y, 80, 26, hwnd, (HMENU)(INT_PTR)IDOK, nullptr, nullptr);
        SendMessage(hOK, WM_SETFONT, (WPARAM)bodyFont, TRUE);

        // Center on parent
        HWND parent = GetParent(hwnd);
        if (parent) {
            RECT prc, drc;
            GetWindowRect(parent, &prc);
            GetWindowRect(hwnd, &drc);
            int cx = prc.left + (prc.right - prc.left - (drc.right - drc.left)) / 2;
            int cy = prc.top + (prc.bottom - prc.top - (drc.bottom - drc.top)) / 3;
            SetWindowPos(hwnd, nullptr, cx, cy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        return TRUE;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hwnd, 0);
            return TRUE;
        }
        break;

    case WM_NOTIFY: {
        auto* nmhdr = reinterpret_cast<NMHDR*>(lParam);
        if (nmhdr->code == NM_CLICK || nmhdr->code == NM_RETURN) {
            if (nmhdr->idFrom == IDC_ABOUT_LINK) {
                auto* link = reinterpret_cast<NMLINK*>(lParam);
                ShellExecuteW(nullptr, L"open", link->item.szUrl, nullptr, nullptr, SW_SHOW);
                return TRUE;
            }
        }
        break;
    }

    case WM_CLOSE:
        EndDialog(hwnd, 0);
        return TRUE;
    }

    return FALSE;
}

void MainWindow::on_about() {
    struct DialogTemplate {
        DWORD style;
        DWORD exStyle;
        WORD cdit;
        short x, y, cx, cy;
        WORD menu;
        WORD windowClass;
        wchar_t title[32];
    };

    DialogTemplate tmpl = {};
    tmpl.style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    tmpl.cdit = 0;
    tmpl.x = 0; tmpl.y = 0;
    tmpl.cx = 180; tmpl.cy = 140;
    wcscpy_s(tmpl.title, L"About Phograph");

    DialogBoxIndirectParamW(
        hInst_,
        reinterpret_cast<LPCDLGTEMPLATEW>(&tmpl),
        hwnd_,
        AboutDlgProc,
        0);
}

void MainWindow::on_docs() {
    ShellExecuteW(nullptr, L"open",
        L"https://avwohl.github.io/phograph/", nullptr, nullptr, SW_SHOW);
}

void MainWindow::on_browse_examples() {
    std::string json = show_example_browser(hwnd_, hInst_);
    if (!json.empty()) {
        if (app_.load_project(json)) {
            update_ui();
            app_.set_status("Example loaded");
            update_status();
        }
    }
}

// -----------------------------------------------------------------------
// UI updates
// -----------------------------------------------------------------------

void MainWindow::update_ui() {
    populate_sidebar();
    update_console();
    update_status();
    if (canvas_) canvas_->invalidate();

    // Update title bar
    std::wstring title = L"Phograph";
    if (!app_.project_name().empty()) {
        title += L" - ";
        std::string pn = app_.project_name();
        title += std::wstring(pn.begin(), pn.end());
    }
    SetWindowTextW(hwnd_, title.c_str());
}

void MainWindow::update_console() {
    if (!console_) return;
    std::string text = app_.console_output();
    // Convert to wide string for the edit control
    int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0);
    std::wstring wtext(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), &wtext[0], wlen);
    // Replace \n with \r\n for edit control
    std::wstring formatted;
    formatted.reserve(wtext.size() + wtext.size() / 10);
    for (wchar_t ch : wtext) {
        if (ch == L'\n') formatted += L"\r\n";
        else formatted += ch;
    }
    SetWindowTextW(console_, formatted.c_str());
    // Scroll to bottom
    SendMessage(console_, EM_SETSEL, (WPARAM)formatted.size(), (LPARAM)formatted.size());
    SendMessage(console_, EM_SCROLLCARET, 0, 0);
}

void MainWindow::update_sidebar() {
    populate_sidebar();
}

void MainWindow::update_status() {
    if (!statusbar_) return;
    std::string msg = app_.status_message();
    std::wstring wmsg(msg.begin(), msg.end());
    SendMessageW(statusbar_, SB_SETTEXTW, 0, (LPARAM)wmsg.c_str());
}

// -----------------------------------------------------------------------
// Fuzzy finder dialog
// -----------------------------------------------------------------------

void MainWindow::show_fuzzy_finder() {
    auto names = app_.all_primitive_names();
    std::string result = show_fuzzy_finder_dialog(hwnd_, hInst_, names);

    if (!result.empty() && app_.current_graph()) {
        // Add node to graph
        VisualNode vn;
        vn.id = (uint32_t)(app_.current_graph()->nodes.size() + 100);
        vn.label = result;
        vn.x = 300;
        vn.y = 200;

        // Determine type and pin counts
        if (result.substr(0, 4) == "new ") {
            vn.node_type = "instance_generator";
            vn.num_outputs = 1;
        } else if (result.substr(0, 4) == "get ") {
            vn.node_type = "get";
            vn.num_inputs = 1;
            vn.num_outputs = 2;
        } else if (result.substr(0, 4) == "set ") {
            vn.node_type = "set";
            vn.num_inputs = 2;
            vn.num_outputs = 1;
        } else if (result.find('/') != std::string::npos) {
            vn.node_type = "method_call";
            vn.num_inputs = 1;
            vn.num_outputs = 1;
            // Look up actual pin counts
            auto parts_pos = result.find('/');
            std::string method_name = result.substr(parts_pos + 1);
            if (auto* mi = app_.find_method_info(method_name)) {
                vn.num_inputs = mi->num_inputs;
                vn.num_outputs = mi->num_outputs;
            }
        } else {
            vn.node_type = "primitive";
            vn.num_inputs = 1;
            vn.num_outputs = 1;
            // Common primitives with known pin counts
            if (result == "+" || result == "-" || result == "*" || result == "/" ||
                result == "=" || result == "<" || result == ">" || result == "<=" ||
                result == ">=" || result == "!=" || result == "and" || result == "or" ||
                result == "concat" || result == "mod") {
                vn.num_inputs = 2;
                vn.num_outputs = 1;
            } else if (result == "not" || result == "abs" || result == "round" ||
                       result == "floor" || result == "ceil" || result == "sqrt" ||
                       result == "length" || result == "log" || result == "inspect" ||
                       result == "to-string" || result == "to-integer" || result == "to-real" ||
                       result == "typeof" || result == "empty?" || result == "print") {
                vn.num_inputs = 1;
                vn.num_outputs = 1;
            } else if (result == "if") {
                vn.num_inputs = 3;
                vn.num_outputs = 1;
            } else if (result == "dict-set") {
                vn.num_inputs = 3;
                vn.num_outputs = 1;
            } else if (result == "dict-create") {
                vn.num_inputs = 0;
                vn.num_outputs = 1;
            }
        }

        float label_w = (float)vn.label.size() * 9.5f + 40.0f;
        vn.width = (std::max)(160.0f, label_w);
        vn.height = 50.0f;

        app_.current_graph()->nodes.push_back(vn);
        if (canvas_) canvas_->invalidate();
    }
}

// -----------------------------------------------------------------------
// Window procedure
// -----------------------------------------------------------------------

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* self = nullptr;

    if (msg == WM_CREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = static_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_SIZE:
        self->layout_controls();
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_FILE_NEW:        self->on_file_new(); break;
        case IDM_FILE_OPEN:       self->on_file_open(); break;
        case IDM_FILE_SAVE:       self->on_file_save(); break;
        case IDM_FILE_SAVEAS:     self->on_file_save_as(); break;
        case IDM_FILE_EXIT:       DestroyWindow(hwnd); break;
        case IDM_EDIT_DELETE:     self->on_delete_selected(); break;
        case IDM_EDIT_SELECTALL:  self->on_select_all(); break;
        case IDM_EDIT_DUPLICATE:  self->on_duplicate(); break;
        case IDM_VIEW_SIDEBAR:    self->on_toggle_sidebar(); break;
        case IDM_VIEW_INSPECTOR:  self->on_toggle_inspector(); break;
        case IDM_VIEW_CONSOLE:    self->on_toggle_console(); break;
        case IDM_VIEW_ZOOMIN:     if (self->canvas_) self->canvas_->zoom_in(); break;
        case IDM_VIEW_ZOOMOUT:    if (self->canvas_) self->canvas_->zoom_out(); break;
        case IDM_VIEW_FITWINDOW:  if (self->canvas_) self->canvas_->fit_to_window(); break;
        case IDM_RUN_RUN:         self->on_run(); break;
        case IDM_RUN_DEBUG:       self->on_debug(); break;
        case IDM_RUN_STOP:        self->on_stop(); break;
        case IDM_RUN_CONTINUE:    self->on_debug_continue(); break;
        case IDM_RUN_STEPOVER:    self->on_debug_step_over(); break;
        case IDM_RUN_STEPINTO:    self->on_debug_step_into(); break;
        case IDM_RUN_CLEARCONSOLE:
            self->app_.clear_console();
            self->update_console();
            break;
        case IDM_NODE_ADD:        self->on_add_node(); break;
        case IDM_HELP_EXAMPLES:   self->on_browse_examples(); break;
        case IDM_HELP_DOCS:       self->on_docs(); break;
        case IDM_HELP_ABOUT:      self->on_about(); break;
        }
        return 0;

    case WM_NOTIFY: {
        auto* nmhdr = reinterpret_cast<NMHDR*>(lParam);
        if (nmhdr->idFrom == IDC_SIDEBAR && nmhdr->code == TVN_SELCHANGEDW) {
            auto* nmtv = reinterpret_cast<NMTREEVIEWW*>(lParam);
            // Get selected item text
            wchar_t buf[256] = {};
            TVITEMW tvi = {};
            tvi.hItem = nmtv->itemNew.hItem;
            tvi.mask = TVIF_TEXT;
            tvi.pszText = buf;
            tvi.cchTextMax = 256;
            TreeView_GetItem(self->sidebar_, &tvi);

            // Extract method name (strip pin count info)
            std::wstring wtext = buf;
            auto paren = wtext.find(L" (");
            if (paren != std::wstring::npos) {
                std::wstring wname = wtext.substr(0, paren);
                int needed = WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), (int)wname.size(), nullptr, 0, nullptr, nullptr);
                std::string name(needed, '\0');
                WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), (int)wname.size(), &name[0], needed, nullptr, nullptr);
                self->app_.select_method(name, 0);
                if (self->canvas_) {
                    self->canvas_->invalidate();
                    self->canvas_->fit_to_window();
                }
                self->update_status();
            }
        }
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}
