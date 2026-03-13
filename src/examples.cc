#include "examples.h"
#include "resource.h"
#include "version.h"
#include <winhttp.h>
#include <shlobj.h>
#include <commctrl.h>
#include <shellapi.h>
#include <algorithm>
#include <cstdio>

#pragma comment(lib, "winhttp.lib")

// -----------------------------------------------------------------------
// Minimal JSON parser for catalog.json
// -----------------------------------------------------------------------

class CatalogParser {
    const char* p_;
    const char* end_;

    void ws() { while (p_ < end_ && (*p_ == ' ' || *p_ == '\n' || *p_ == '\r' || *p_ == '\t')) p_++; }

    bool eat(char c) {
        ws();
        if (p_ < end_ && *p_ == c) { p_++; return true; }
        return false;
    }

    std::string str() {
        ws();
        if (p_ >= end_ || *p_ != '"') return "";
        p_++; // skip opening quote
        std::string s;
        while (p_ < end_ && *p_ != '"') {
            if (*p_ == '\\') {
                p_++;
                if (p_ < end_) {
                    switch (*p_) {
                        case '"': s += '"'; break;
                        case '\\': s += '\\'; break;
                        case 'n': s += '\n'; break;
                        case 't': s += '\t'; break;
                        case '/': s += '/'; break;
                        default: s += *p_; break;
                    }
                }
            } else {
                s += *p_;
            }
            p_++;
        }
        if (p_ < end_) p_++; // skip closing quote
        return s;
    }

    void skip_value() {
        ws();
        if (p_ >= end_) return;
        if (*p_ == '"') { str(); return; }
        if (*p_ == '{') { skip_matched('{', '}'); return; }
        if (*p_ == '[') { skip_matched('[', ']'); return; }
        // number, true, false, null
        while (p_ < end_ && *p_ != ',' && *p_ != '}' && *p_ != ']') p_++;
    }

    void skip_matched(char open, char close) {
        int depth = 1;
        p_++; // skip opening
        while (p_ < end_ && depth > 0) {
            if (*p_ == '"') {
                p_++;
                while (p_ < end_ && *p_ != '"') { if (*p_ == '\\') p_++; p_++; }
                if (p_ < end_) p_++;
            } else {
                if (*p_ == open) depth++;
                else if (*p_ == close) depth--;
                p_++;
            }
        }
    }

    ExampleEntry parse_example() {
        ExampleEntry ex;
        eat('{');
        while (p_ < end_ && !eat('}')) {
            std::string key = str();
            eat(':');
            if (key == "name") ex.name = str();
            else if (key == "description") ex.description = str();
            else if (key == "file") ex.file = str();
            else skip_value();
            eat(',');
        }
        return ex;
    }

    ExampleCategory parse_category() {
        ExampleCategory cat;
        eat('{');
        while (p_ < end_ && !eat('}')) {
            std::string key = str();
            eat(':');
            if (key == "name") cat.name = str();
            else if (key == "examples") {
                eat('[');
                while (p_ < end_ && !eat(']')) {
                    cat.examples.push_back(parse_example());
                    eat(',');
                }
            } else {
                skip_value();
            }
            eat(',');
        }
        return cat;
    }

public:
    std::vector<ExampleCategory> parse(const std::string& json) {
        p_ = json.c_str();
        end_ = p_ + json.size();
        std::vector<ExampleCategory> result;

        eat('{');
        while (p_ < end_ && !eat('}')) {
            std::string key = str();
            eat(':');
            if (key == "categories") {
                eat('[');
                while (p_ < end_ && !eat(']')) {
                    result.push_back(parse_category());
                    eat(',');
                }
            } else {
                skip_value();
            }
            eat(',');
        }
        return result;
    }
};

// -----------------------------------------------------------------------
// WinHTTP helpers
// -----------------------------------------------------------------------

static std::string winhttp_get(const std::wstring& url) {
    URL_COMPONENTSW uc = {};
    wchar_t host[256] = {};
    wchar_t path[2048] = {};
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host;
    uc.dwHostNameLength = _countof(host);
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = _countof(path);

    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc))
        return "";

    HINTERNET session = WinHttpOpen(L"Phograph/" PHO_VERSION_STRING_W,
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!session) return "";

    HINTERNET connect = WinHttpConnect(session, host, uc.nPort, 0);
    if (!connect) { WinHttpCloseHandle(session); return ""; }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connect, L"GET", path,
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) { WinHttpCloseHandle(connect); WinHttpCloseHandle(session); return ""; }

    // GitHub API requires User-Agent; set Accept for JSON
    WinHttpAddRequestHeaders(request,
        L"User-Agent: Phograph/0.1.0\r\nAccept: application/vnd.github+json\r\n",
        (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

    // Enable automatic redirect following
    DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY,
        &redirect_policy, sizeof(redirect_policy));

    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request, nullptr)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return "";
    }

    std::string result;
    char buffer[8192];
    DWORD bytes_read = 0;
    while (WinHttpReadData(request, buffer, sizeof(buffer), &bytes_read) && bytes_read > 0) {
        result.append(buffer, bytes_read);
        bytes_read = 0;
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return result;
}

static bool winhttp_download_to_file(const std::wstring& url, const std::wstring& filepath) {
    URL_COMPONENTSW uc = {};
    wchar_t host[256] = {};
    wchar_t path[2048] = {};
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host;
    uc.dwHostNameLength = _countof(host);
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = _countof(path);

    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc))
        return false;

    HINTERNET session = WinHttpOpen(L"Phograph/" PHO_VERSION_STRING_W,
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!session) return false;

    HINTERNET connect = WinHttpConnect(session, host, uc.nPort, 0);
    if (!connect) { WinHttpCloseHandle(session); return false; }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connect, L"GET", path,
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) { WinHttpCloseHandle(connect); WinHttpCloseHandle(session); return false; }

    WinHttpAddRequestHeaders(request,
        L"User-Agent: Phograph/0.1.0\r\nAccept: application/octet-stream\r\n",
        (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

    DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY,
        &redirect_policy, sizeof(redirect_policy));

    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request, nullptr)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    FILE* f = _wfopen(filepath.c_str(), L"wb");
    if (!f) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    char buffer[8192];
    DWORD bytes_read = 0;
    bool ok = true;
    while (WinHttpReadData(request, buffer, sizeof(buffer), &bytes_read) && bytes_read > 0) {
        if (fwrite(buffer, 1, bytes_read, f) != bytes_read) { ok = false; break; }
        bytes_read = 0;
    }
    fclose(f);

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return ok;
}

// -----------------------------------------------------------------------
// Cache management
// -----------------------------------------------------------------------

static std::wstring get_cache_dir() {
    wchar_t* appdata = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &appdata))) {
        std::wstring dir = appdata;
        CoTaskMemFree(appdata);
        return dir + L"\\Phograph\\examples";
    }
    return L"";
}

static void ensure_directory(const std::wstring& path) {
    // Create each component
    for (size_t i = 3; i < path.size(); i++) {
        if (path[i] == L'\\' || path[i] == L'/') {
            CreateDirectoryW(path.substr(0, i).c_str(), nullptr);
        }
    }
    CreateDirectoryW(path.c_str(), nullptr);
}

static std::string read_file_utf8(const std::wstring& path) {
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) return "";
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string data(sz, '\0');
    fread(&data[0], 1, sz, f);
    fclose(f);
    return data;
}

// Find the asset download URL from the GitHub releases API response
static std::string find_asset_url(const std::string& api_json) {
    // Search for "browser_download_url" associated with an asset whose name
    // contains "phograph-examples"
    size_t pos = 0;
    while ((pos = api_json.find("\"browser_download_url\"", pos)) != std::string::npos) {
        // Find the URL value
        size_t colon = api_json.find(':', pos + 21);
        if (colon == std::string::npos) break;
        size_t qstart = api_json.find('"', colon + 1);
        if (qstart == std::string::npos) break;
        qstart++;
        size_t qend = api_json.find('"', qstart);
        if (qend == std::string::npos) break;
        std::string url = api_json.substr(qstart, qend - qstart);
        if (url.find("phograph-examples") != std::string::npos) {
            return url;
        }
        pos = qend;
    }
    return "";
}

static std::wstring ensure_examples_cached(HWND parent) {
    std::wstring dir = get_cache_dir();
    if (dir.empty()) return L"";

    std::wstring catalog_path = dir + L"\\catalog.json";

    // Already cached?
    if (GetFileAttributesW(catalog_path.c_str()) != INVALID_FILE_ATTRIBUTES)
        return dir;

    // Download from GitHub releases
    HCURSOR old_cursor = SetCursor(LoadCursor(nullptr, IDC_WAIT));

    // 1. Query GitHub API for latest release
    std::string api_response = winhttp_get(
        L"https://api.github.com/repos/avwohl/phograph/releases/latest");

    if (api_response.empty()) {
        SetCursor(old_cursor);
        MessageBoxW(parent,
            L"Could not connect to GitHub to download examples.\n"
            L"Check your internet connection and try again.",
            L"Download Failed", MB_OK | MB_ICONWARNING);
        return L"";
    }

    // 2. Find the examples tarball URL
    std::string asset_url = find_asset_url(api_response);
    if (asset_url.empty()) {
        SetCursor(old_cursor);
        MessageBoxW(parent,
            L"Could not find example programs in the latest release.",
            L"Download Failed", MB_OK | MB_ICONWARNING);
        return L"";
    }

    // 3. Download tar.gz to temp file
    wchar_t temp_path[MAX_PATH];
    GetTempPathW(MAX_PATH, temp_path);
    std::wstring tar_path = std::wstring(temp_path) + L"phograph-examples.tar.gz";

    std::wstring wurl(asset_url.begin(), asset_url.end());
    if (!winhttp_download_to_file(wurl, tar_path)) {
        SetCursor(old_cursor);
        MessageBoxW(parent,
            L"Failed to download example programs.",
            L"Download Failed", MB_OK | MB_ICONWARNING);
        return L"";
    }

    // 4. Create cache directory
    ensure_directory(dir);

    // 5. Extract with tar.exe (built into Windows 10+)
    std::wstring cmd = L"tar.exe -xzf \"" + tar_path + L"\" -C \"" + dir + L"\"";
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    if (CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 30000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    // 6. Cleanup temp file
    DeleteFileW(tar_path.c_str());

    SetCursor(old_cursor);

    // 7. Verify extraction. The tar.gz may have extracted files into a
    //    subdirectory (e.g., "examples/"). Check for catalog.json.
    if (GetFileAttributesW(catalog_path.c_str()) != INVALID_FILE_ATTRIBUTES)
        return dir;

    // Check one level deep for a subdirectory containing catalog.json
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0) {
                std::wstring sub = dir + L"\\" + fd.cFileName + L"\\catalog.json";
                if (GetFileAttributesW(sub.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    FindClose(hFind);
                    return dir + L"\\" + fd.cFileName;
                }
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }

    MessageBoxW(parent,
        L"Examples downloaded but catalog.json was not found.\n"
        L"The archive format may have changed.",
        L"Error", MB_OK | MB_ICONWARNING);
    return L"";
}

static std::vector<ExampleCategory> load_catalog(const std::wstring& dir) {
    std::string json = read_file_utf8(dir + L"\\catalog.json");
    if (json.empty()) return {};
    CatalogParser parser;
    return parser.parse(json);
}

static std::string read_example(const std::wstring& dir, const std::string& filename) {
    std::wstring wfile(filename.begin(), filename.end());
    return read_file_utf8(dir + L"\\" + wfile);
}

// -----------------------------------------------------------------------
// Example browser dialog
// -----------------------------------------------------------------------

struct BrowserState {
    std::vector<ExampleCategory> catalog;
    std::wstring examples_dir;
    std::string result_json;        // selected example's JSON content
    HWND tree;
    HWND desc;
    HFONT font;

    // Map HTREEITEM to category/example indices
    struct ItemInfo {
        int cat;        // category index (-1 = category node)
        int example;    // example index within category
    };
    std::vector<std::pair<HTREEITEM, ItemInfo>> items;

    const ExampleEntry* find_entry(HTREEITEM hItem) const {
        for (auto& p : items) {
            if (p.first == hItem && p.second.cat >= 0 && p.second.example >= 0)
                return &catalog[p.second.cat].examples[p.second.example];
        }
        return nullptr;
    }
};

static void browser_populate_tree(BrowserState* state) {
    TreeView_DeleteAllItems(state->tree);
    state->items.clear();

    for (int ci = 0; ci < (int)state->catalog.size(); ci++) {
        auto& cat = state->catalog[ci];
        TVINSERTSTRUCTW tvis = {};
        tvis.hParent = TVI_ROOT;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT | TVIF_CHILDREN;
        tvis.item.cChildren = (int)cat.examples.size();
        std::wstring wname(cat.name.begin(), cat.name.end());
        tvis.item.pszText = (LPWSTR)wname.c_str();
        HTREEITEM hCat = TreeView_InsertItem(state->tree, &tvis);
        state->items.push_back({hCat, {ci, -1}});

        for (int ei = 0; ei < (int)cat.examples.size(); ei++) {
            auto& ex = cat.examples[ei];
            TVINSERTSTRUCTW evis = {};
            evis.hParent = hCat;
            evis.hInsertAfter = TVI_LAST;
            evis.item.mask = TVIF_TEXT;
            std::wstring wex(ex.name.begin(), ex.name.end());
            evis.item.pszText = (LPWSTR)wex.c_str();
            HTREEITEM hEx = TreeView_InsertItem(state->tree, &evis);
            state->items.push_back({hEx, {ci, ei}});
        }

        TreeView_Expand(state->tree, hCat, TVE_EXPAND);
    }
}

static void browser_on_select(BrowserState* state, HTREEITEM hItem) {
    const ExampleEntry* entry = state->find_entry(hItem);
    if (entry) {
        std::wstring wdesc(entry->description.begin(), entry->description.end());
        SetWindowTextW(state->desc, wdesc.c_str());
    } else {
        // Category selected - show summary
        for (auto& p : state->items) {
            if (p.first == hItem && p.second.example < 0 && p.second.cat >= 0) {
                auto& cat = state->catalog[p.second.cat];
                std::wstring summary = std::wstring(cat.name.begin(), cat.name.end()) +
                    L"\n\n" + std::to_wstring(cat.examples.size()) + L" examples";
                SetWindowTextW(state->desc, summary.c_str());
                break;
            }
        }
    }
}

static HTREEITEM browser_get_selected_example(BrowserState* state) {
    HTREEITEM sel = TreeView_GetSelection(state->tree);
    if (!sel) return nullptr;
    if (state->find_entry(sel)) return sel;
    return nullptr;
}

static INT_PTR CALLBACK BrowserDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    BrowserState* state = reinterpret_cast<BrowserState*>(
        GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_INITDIALOG: {
        state = reinterpret_cast<BrowserState*>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);

        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right, h = rc.bottom;

        state->font = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

        int tree_w = 200;
        int btn_h = 36;
        int pad = 8;

        // TreeView on the left
        state->tree = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, nullptr,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | TVS_HASLINES | TVS_HASBUTTONS |
            TVS_LINESATROOT | TVS_SHOWSELALWAYS,
            pad, pad, tree_w, h - btn_h - pad * 3,
            hwnd, (HMENU)(INT_PTR)IDC_EXAMPLE_TREE, nullptr, nullptr);
        SendMessage(state->tree, WM_SETFONT, (WPARAM)state->font, TRUE);

        // Description on the right
        state->desc = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC",
            L"Select an example to see its description.",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            tree_w + pad * 2, pad, w - tree_w - pad * 3, h - btn_h - pad * 3,
            hwnd, (HMENU)(INT_PTR)IDC_EXAMPLE_DESC, nullptr, nullptr);
        SendMessage(state->desc, WM_SETFONT, (WPARAM)state->font, TRUE);

        // Open button
        HWND hOpen = CreateWindowExW(0, L"BUTTON", L"Open",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            w - 180, h - btn_h - pad, 80, 28,
            hwnd, (HMENU)(INT_PTR)IDOK, nullptr, nullptr);
        SendMessage(hOpen, WM_SETFONT, (WPARAM)state->font, TRUE);

        // Cancel button
        HWND hCancel = CreateWindowExW(0, L"BUTTON", L"Cancel",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            w - 90, h - btn_h - pad, 80, 28,
            hwnd, (HMENU)(INT_PTR)IDCANCEL, nullptr, nullptr);
        SendMessage(hCancel, WM_SETFONT, (WPARAM)state->font, TRUE);

        // Populate tree
        browser_populate_tree(state);

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
        if (LOWORD(wParam) == IDOK) {
            HTREEITEM sel = browser_get_selected_example(state);
            if (sel) {
                const ExampleEntry* entry = state->find_entry(sel);
                if (entry) {
                    state->result_json = read_example(state->examples_dir, entry->file);
                    if (!state->result_json.empty()) {
                        EndDialog(hwnd, IDOK);
                    } else {
                        MessageBoxW(hwnd, L"Could not read example file.",
                            L"Error", MB_OK | MB_ICONWARNING);
                    }
                }
            }
            return TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
        }
        break;

    case WM_NOTIFY: {
        auto* nmhdr = reinterpret_cast<NMHDR*>(lParam);
        if (nmhdr->idFrom == IDC_EXAMPLE_TREE) {
            if (nmhdr->code == TVN_SELCHANGEDW) {
                auto* nmtv = reinterpret_cast<NMTREEVIEWW*>(lParam);
                browser_on_select(state, nmtv->itemNew.hItem);
            }
            if (nmhdr->code == NM_DBLCLK) {
                HTREEITEM sel = browser_get_selected_example(state);
                if (sel) {
                    const ExampleEntry* entry = state->find_entry(sel);
                    if (entry) {
                        state->result_json = read_example(state->examples_dir, entry->file);
                        if (!state->result_json.empty())
                            EndDialog(hwnd, IDOK);
                    }
                }
            }
        }
        return TRUE;
    }

    case WM_CLOSE:
        EndDialog(hwnd, IDCANCEL);
        return TRUE;
    }

    return FALSE;
}

// -----------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------

std::string show_example_browser(HWND parent, HINSTANCE hInst) {
    // Ensure examples are downloaded and cached
    std::wstring dir = ensure_examples_cached(parent);
    if (dir.empty()) return "";

    // Load catalog
    std::vector<ExampleCategory> catalog = load_catalog(dir);
    if (catalog.empty()) {
        MessageBoxW(parent, L"No examples found in catalog.",
            L"Browse Examples", MB_OK | MB_ICONINFORMATION);
        return "";
    }

    // Show browser dialog
    BrowserState state;
    state.catalog = std::move(catalog);
    state.examples_dir = dir;

    struct BrowserDialogTemplate {
        DWORD style;
        DWORD exStyle;
        WORD cdit;
        short x, y, cx, cy;
        WORD menu;
        WORD windowClass;
        wchar_t title[32];
    };

    BrowserDialogTemplate tmpl = {};
    tmpl.style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    tmpl.cdit = 0;
    tmpl.x = 0; tmpl.y = 0;
    tmpl.cx = 280; tmpl.cy = 200;
    wcscpy_s(tmpl.title, L"Browse Examples");

    INT_PTR result = DialogBoxIndirectParamW(
        hInst,
        reinterpret_cast<LPCDLGTEMPLATEW>(&tmpl),
        parent,
        BrowserDlgProc,
        reinterpret_cast<LPARAM>(&state));

    if (result == IDOK)
        return state.result_json;
    return "";
}
