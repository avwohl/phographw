#include "dialogs.h"
#include <windowsx.h>
#include <commctrl.h>
#include <algorithm>

// Fuzzy match scoring (port of pho_fuzzy.h logic)
static int fuzzy_score(const std::string& query, const std::string& text) {
    if (query.empty()) return 0;

    int score = 0;
    size_t qi = 0;
    bool prev_match = false;

    for (size_t ti = 0; ti < text.size() && qi < query.size(); ti++) {
        char qc = (char)tolower(query[qi]);
        char tc = (char)tolower(text[ti]);
        if (qc == tc) {
            score += 10;
            if (prev_match) score += 5;       // consecutive match bonus
            if (ti == 0 || text[ti-1] == '-' || text[ti-1] == '_' || text[ti-1] == ' ')
                score += 8;                    // word boundary bonus
            prev_match = true;
            qi++;
        } else {
            prev_match = false;
        }
    }

    if (qi < query.size()) return -1; // Not all query chars matched

    // Shorter text gets bonus
    score += (std::max)(0, 20 - (int)text.size());
    return score;
}

struct FuzzyMatch {
    int index;
    int score;
    std::string text;
};

// -----------------------------------------------------------------------
// Fuzzy Finder Dialog
// -----------------------------------------------------------------------

// Dialog state (passed via LPARAM)
struct FuzzyFinderState {
    const std::vector<std::string>* items;
    std::string result;
    HWND edit;
    HWND listbox;
    std::vector<FuzzyMatch> matches;
};

static void update_fuzzy_list(FuzzyFinderState* state) {
    SendMessage(state->listbox, LB_RESETCONTENT, 0, 0);

    // Get query text
    wchar_t wbuf[256] = {};
    GetWindowTextW(state->edit, wbuf, 256);
    int wlen = (int)wcslen(wbuf);
    char buf[256] = {};
    WideCharToMultiByte(CP_UTF8, 0, wbuf, wlen, buf, 256, nullptr, nullptr);
    std::string query = buf;

    // Score and filter
    state->matches.clear();
    for (int i = 0; i < (int)state->items->size(); i++) {
        const auto& item = (*state->items)[i];
        if (query.empty()) {
            state->matches.push_back({i, 0, item});
        } else {
            int s = fuzzy_score(query, item);
            if (s > 0) {
                state->matches.push_back({i, s, item});
            }
        }
    }

    // Sort by score (descending)
    if (!query.empty()) {
        std::sort(state->matches.begin(), state->matches.end(),
            [](const FuzzyMatch& a, const FuzzyMatch& b) { return a.score > b.score; });
    }

    // Populate list (max 50 items)
    int count = (std::min)((int)state->matches.size(), 50);
    for (int i = 0; i < count; i++) {
        std::wstring witem(state->matches[i].text.begin(), state->matches[i].text.end());
        SendMessageW(state->listbox, LB_ADDSTRING, 0, (LPARAM)witem.c_str());
    }

    // Select first item
    if (count > 0) {
        SendMessage(state->listbox, LB_SETCURSEL, 0, 0);
    }
}

static INT_PTR CALLBACK FuzzyFinderProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    FuzzyFinderState* state = reinterpret_cast<FuzzyFinderState*>(
        GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_INITDIALOG: {
        state = reinterpret_cast<FuzzyFinderState*>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);

        // Create controls manually
        int dlg_w = 400, dlg_h = 450;
        RECT rc;
        GetClientRect(hwnd, &rc);

        state->edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            10, 10, rc.right - 20, 24,
            hwnd, (HMENU)1001, nullptr, nullptr);

        state->listbox = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
            10, 44, rc.right - 20, rc.bottom - 54,
            hwnd, (HMENU)1002, nullptr, nullptr);

        // Set focus to edit
        SetFocus(state->edit);

        // Set font
        HFONT font = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        SendMessage(state->edit, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessage(state->listbox, WM_SETFONT, (WPARAM)font, TRUE);

        // Initial populate
        update_fuzzy_list(state);

        // Center dialog
        RECT parent_rc;
        GetWindowRect(GetParent(hwnd), &parent_rc);
        RECT dlg_rc;
        GetWindowRect(hwnd, &dlg_rc);
        int x = parent_rc.left + (parent_rc.right - parent_rc.left - (dlg_rc.right - dlg_rc.left)) / 2;
        int y = parent_rc.top + (parent_rc.bottom - parent_rc.top - (dlg_rc.bottom - dlg_rc.top)) / 3;
        SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

        return FALSE; // We set focus ourselves
    }

    case WM_COMMAND:
        if (HIWORD(wParam) == EN_CHANGE && LOWORD(wParam) == 1001) {
            update_fuzzy_list(state);
            return TRUE;
        }
        if (HIWORD(wParam) == LBN_DBLCLK && LOWORD(wParam) == 1002) {
            int sel = (int)SendMessage(state->listbox, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)state->matches.size()) {
                state->result = state->matches[sel].text;
                EndDialog(hwnd, IDOK);
            }
            return TRUE;
        }
        break;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
        }
        if (wParam == VK_RETURN) {
            int sel = (int)SendMessage(state->listbox, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)state->matches.size()) {
                state->result = state->matches[sel].text;
                EndDialog(hwnd, IDOK);
            }
            return TRUE;
        }
        break;

    case WM_CLOSE:
        EndDialog(hwnd, IDCANCEL);
        return TRUE;

    case WM_SIZE: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        if (state && state->edit) {
            MoveWindow(state->edit, 10, 10, rc.right - 20, 24, TRUE);
            MoveWindow(state->listbox, 10, 44, rc.right - 20, rc.bottom - 54, TRUE);
        }
        return TRUE;
    }
    }

    // Forward keyboard to edit control for navigation
    if (msg == WM_KEYDOWN && state && state->edit) {
        if (wParam == VK_UP || wParam == VK_DOWN) {
            SendMessage(state->listbox, msg, wParam, lParam);
            return TRUE;
        }
    }

    return FALSE;
}

// Subclass proc for the edit control to handle Up/Down/Enter/Escape
static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                          UINT_PTR subclassId, DWORD_PTR refData) {
    FuzzyFinderState* state = reinterpret_cast<FuzzyFinderState*>(refData);
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_UP || wParam == VK_DOWN) {
            SendMessage(state->listbox, msg, wParam, lParam);
            return 0;
        }
        if (wParam == VK_RETURN) {
            int sel = (int)SendMessage(state->listbox, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)state->matches.size()) {
                state->result = state->matches[sel].text;
                EndDialog(GetParent(hwnd), IDOK);
            }
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            EndDialog(GetParent(hwnd), IDCANCEL);
            return 0;
        }
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

std::string show_fuzzy_finder_dialog(HWND parent, HINSTANCE hInst,
                                      const std::vector<std::string>& items) {
    FuzzyFinderState state;
    state.items = &items;

    // Create dialog template in memory
    // We use a minimal dialog template struct
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
    tmpl.style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX;
    tmpl.exStyle = 0;
    tmpl.cdit = 0;
    tmpl.x = 0; tmpl.y = 0;
    tmpl.cx = 200; tmpl.cy = 220;
    tmpl.menu = 0;
    tmpl.windowClass = 0;
    wcscpy_s(tmpl.title, L"Add Node (Ctrl+K)");

    INT_PTR result = DialogBoxIndirectParamW(
        hInst,
        reinterpret_cast<LPCDLGTEMPLATEW>(&tmpl),
        parent,
        FuzzyFinderProc,
        reinterpret_cast<LPARAM>(&state));

    // After dialog creation, subclass the edit control
    // (This is done in WM_INITDIALOG via a timer or direct call)

    if (result == IDOK) return state.result;
    return "";
}
