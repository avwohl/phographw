#include "pho_platform.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winhttp.h>
#include <dwrite.h>
#include <shlwapi.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <map>
#include <mutex>
#include <atomic>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "user32.lib")

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

// ---- Timer bookkeeping ----------------------------------------------------

struct TimerEntry {
    HANDLE              queue_timer;
    void              (*callback)(void* ctx);
    void*               ctx;
};

static std::mutex              g_timer_mutex;
static std::map<uint64_t, TimerEntry> g_timers;
static std::atomic<uint64_t>   g_next_timer_id{1};

static VOID CALLBACK timer_queue_callback(PVOID parameter, BOOLEAN /*timer_or_wait_fired*/) {
    uint64_t id = reinterpret_cast<uint64_t>(parameter);
    void (*cb)(void*) = nullptr;
    void* ctx = nullptr;

    {
        std::lock_guard<std::mutex> lock(g_timer_mutex);
        auto it = g_timers.find(id);
        if (it != g_timers.end()) {
            cb  = it->second.callback;
            ctx = it->second.ctx;
            // One-shot: remove after firing.
            DeleteTimerQueueTimer(nullptr, it->second.queue_timer, nullptr);
            g_timers.erase(it);
        }
    }

    if (cb) {
        cb(ctx);
    }
}

// ---- DirectWrite singleton ------------------------------------------------

static IDWriteFactory* g_dwrite_factory = nullptr;
static std::once_flag  g_dwrite_init_flag;

static IDWriteFactory* get_dwrite_factory() {
    std::call_once(g_dwrite_init_flag, []() {
        HRESULT hr = DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(&g_dwrite_factory));
        if (FAILED(hr)) {
            g_dwrite_factory = nullptr;
        }
    });
    return g_dwrite_factory;
}

// ---- High-resolution time -------------------------------------------------

static double get_qpc_frequency() {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return static_cast<double>(freq.QuadPart);
}

static double get_epoch_offset() {
    // Compute the offset between QPC zero and Unix epoch.
    // 1) Get current wall-clock time as seconds since Unix epoch.
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    // FILETIME is 100-ns intervals since 1601-01-01.  Unix epoch is
    // 1970-01-01.  Difference is 11644473600 seconds.
    ULARGE_INTEGER uli;
    uli.LowPart  = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    double wall = static_cast<double>(uli.QuadPart) / 1e7 - 11644473600.0;

    // 2) Get the current QPC value in seconds.
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    double qpc_sec = static_cast<double>(counter.QuadPart) / get_qpc_frequency();

    return wall - qpc_sec;
}

// Lazy-init singletons for time.
static double g_qpc_freq   = 0.0;
static double g_epoch_offset = 0.0;
static std::once_flag g_time_init_flag;

static void ensure_time_init() {
    std::call_once(g_time_init_flag, []() {
        g_qpc_freq    = get_qpc_frequency();
        g_epoch_offset = get_epoch_offset();
    });
}

// ---- UTF-8 / UTF-16 conversion -------------------------------------------

static std::wstring utf8_to_utf16(const char* utf8, int utf8_len = -1) {
    if (!utf8 || (utf8_len == 0)) return std::wstring();
    int needed = MultiByteToWideChar(CP_UTF8, 0, utf8, utf8_len, nullptr, 0);
    if (needed <= 0) return std::wstring();
    std::wstring result(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8, utf8_len, &result[0], needed);
    // If utf8_len was -1, the result includes a NUL terminator in the count.
    if (utf8_len == -1 && !result.empty() && result.back() == L'\0')
        result.pop_back();
    return result;
}

static std::string utf16_to_utf8(const wchar_t* utf16, int utf16_len = -1) {
    if (!utf16 || (utf16_len == 0)) return std::string();
    int needed = WideCharToMultiByte(CP_UTF8, 0, utf16, utf16_len, nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return std::string();
    std::string result(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, utf16, utf16_len, &result[0], needed, nullptr, nullptr);
    if (utf16_len == -1 && !result.empty() && result.back() == '\0')
        result.pop_back();
    return result;
}

// ---- WinHTTP helper -------------------------------------------------------

struct ParsedUrl {
    bool        is_https;
    std::wstring host;
    INTERNET_PORT port;
    std::wstring path;   // includes query string
};

static bool parse_url(const char* url, ParsedUrl& out) {
    std::wstring wurl = utf8_to_utf16(url);

    URL_COMPONENTS components = {};
    components.dwStructSize = sizeof(components);

    wchar_t host_buf[256] = {};
    wchar_t path_buf[2048] = {};

    components.lpszHostName     = host_buf;
    components.dwHostNameLength = _countof(host_buf);
    components.lpszUrlPath      = path_buf;
    components.dwUrlPathLength  = _countof(path_buf);

    if (!WinHttpCrackUrl(wurl.c_str(), static_cast<DWORD>(wurl.size()), 0, &components))
        return false;

    out.is_https = (components.nScheme == INTERNET_SCHEME_HTTPS);
    out.host     = host_buf;
    out.port     = components.nPort;
    out.path     = path_buf;
    if (out.path.empty()) out.path = L"/";

    return true;
}

static int winhttp_request(const char* method_a,
                           const char* url,
                           const void* send_body,
                           size_t send_body_len,
                           const char* content_type,
                           char** out_body,
                           size_t* out_len) {
    ParsedUrl pu;
    if (!parse_url(url, pu)) return -1;

    HINTERNET session = WinHttpOpen(L"Phograph/1.0",
                                    WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS,
                                    0);
    if (!session) return -1;

    // 30-second timeouts: resolve, connect, send, receive.
    WinHttpSetTimeouts(session, 30000, 30000, 30000, 30000);

    HINTERNET connection = WinHttpConnect(session, pu.host.c_str(), pu.port, 0);
    if (!connection) {
        WinHttpCloseHandle(session);
        return -1;
    }

    std::wstring wmethod = utf8_to_utf16(method_a);
    DWORD flags = pu.is_https ? WINHTTP_FLAG_SECURE : 0;

    HINTERNET request = WinHttpOpenRequest(connection,
                                           wmethod.c_str(),
                                           pu.path.c_str(),
                                           nullptr,
                                           WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           flags);
    if (!request) {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return -1;
    }

    // Add Content-Type header for POST if provided.
    if (content_type && content_type[0]) {
        std::wstring header = L"Content-Type: " + utf8_to_utf16(content_type);
        WinHttpAddRequestHeaders(request, header.c_str(),
                                 static_cast<DWORD>(header.size()),
                                 WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    }

    BOOL ok = WinHttpSendRequest(request,
                                 WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                 const_cast<void*>(send_body),
                                 static_cast<DWORD>(send_body_len),
                                 static_cast<DWORD>(send_body_len),
                                 0);
    if (!ok) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return -1;
    }

    ok = WinHttpReceiveResponse(request, nullptr);
    if (!ok) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return -1;
    }

    // Read status code.
    DWORD status_code = 0;
    DWORD sc_size = sizeof(status_code);
    WinHttpQueryHeaders(request,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX,
                        &status_code, &sc_size, WINHTTP_NO_HEADER_INDEX);

    // Read response body.
    std::string body;
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) break;
        if (available == 0) break;

        char* chunk = static_cast<char*>(malloc(available));
        if (!chunk) break;

        DWORD read = 0;
        if (WinHttpReadData(request, chunk, available, &read)) {
            body.append(chunk, read);
        }
        free(chunk);
        if (read == 0) break;
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);

    if (out_body) {
        *out_body = static_cast<char*>(malloc(body.size() + 1));
        if (*out_body) {
            memcpy(*out_body, body.data(), body.size());
            (*out_body)[body.size()] = '\0';
        }
    }
    if (out_len) {
        *out_len = body.size();
    }

    return static_cast<int>(status_code);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// C API implementation
// ---------------------------------------------------------------------------

extern "C" {

char* pho_platform_read_file(const char* path, size_t* out_size) {
    if (!path) return nullptr;

    FILE* f = fopen(path, "rb");
    if (!f) return nullptr;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return nullptr;
    }
    long length = ftell(f);
    if (length < 0) {
        fclose(f);
        return nullptr;
    }
    rewind(f);

    size_t file_size = static_cast<size_t>(length);
    char* buffer = static_cast<char*>(malloc(file_size + 1));
    if (!buffer) {
        fclose(f);
        return nullptr;
    }

    size_t read_count = fread(buffer, 1, file_size, f);
    fclose(f);

    if (read_count != file_size) {
        free(buffer);
        return nullptr;
    }

    buffer[file_size] = '\0';

    if (out_size) {
        *out_size = file_size;
    }
    return buffer;
}

int pho_platform_write_file(const char* path, const void* data, size_t size) {
    if (!path) return -1;

    FILE* f = fopen(path, "wb");
    if (!f) return -1;

    if (size > 0 && data) {
        size_t written = fwrite(data, 1, size, f);
        if (written != size) {
            fclose(f);
            return -1;
        }
    }

    fclose(f);
    return 0;
}

int pho_platform_file_exists(const char* path) {
    if (!path) return 0;
    DWORD attrs = GetFileAttributesA(path);
    return (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) ? 1 : 0;
}

void pho_platform_free(void* ptr) {
    free(ptr);
}

double pho_platform_time_now(void) {
    ensure_time_init();

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    double qpc_sec = static_cast<double>(counter.QuadPart) / g_qpc_freq;

    return g_epoch_offset + qpc_sec;
}

uint64_t pho_platform_timer_after(double delay_seconds, void (*callback)(void* ctx), void* ctx) {
    if (!callback || delay_seconds < 0.0) return 0;

    uint64_t id = g_next_timer_id.fetch_add(1, std::memory_order_relaxed);

    HANDLE queue_timer = nullptr;
    DWORD due_time_ms = static_cast<DWORD>(delay_seconds * 1000.0);
    if (due_time_ms == 0 && delay_seconds > 0.0) due_time_ms = 1;

    BOOL ok = CreateTimerQueueTimer(
        &queue_timer,
        nullptr, // default timer queue
        timer_queue_callback,
        reinterpret_cast<PVOID>(id),
        due_time_ms,
        0,       // period = 0 means one-shot
        WT_EXECUTEDEFAULT);

    if (!ok) return 0;

    {
        std::lock_guard<std::mutex> lock(g_timer_mutex);
        g_timers[id] = {queue_timer, callback, ctx};
    }

    return id;
}

void pho_platform_timer_cancel(uint64_t timer_id) {
    std::lock_guard<std::mutex> lock(g_timer_mutex);
    auto it = g_timers.find(timer_id);
    if (it != g_timers.end()) {
        DeleteTimerQueueTimer(nullptr, it->second.queue_timer, INVALID_HANDLE_VALUE);
        g_timers.erase(it);
    }
}

void pho_platform_measure_text(const char* text,
                               const char* font_name,
                               float font_size,
                               float* out_width,
                               float* out_height) {
    if (out_width)  *out_width  = 0.0f;
    if (out_height) *out_height = 0.0f;

    if (!text || !text[0]) return;

    IDWriteFactory* factory = get_dwrite_factory();
    if (!factory) return;

    const wchar_t* default_font = L"Segoe UI";
    std::wstring wfont;
    if (font_name && font_name[0]) {
        wfont = utf8_to_utf16(font_name);
    }
    const wchar_t* font_family = wfont.empty() ? default_font : wfont.c_str();

    IDWriteTextFormat* text_format = nullptr;
    HRESULT hr = factory->CreateTextFormat(
        font_family,
        nullptr, // font collection (system default)
        DWRITE_FONT_WEIGHT_REGULAR,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        font_size,
        L"en-us",
        &text_format);

    if (FAILED(hr) || !text_format) return;

    std::wstring wtext = utf8_to_utf16(text);

    IDWriteTextLayout* text_layout = nullptr;
    hr = factory->CreateTextLayout(
        wtext.c_str(),
        static_cast<UINT32>(wtext.size()),
        text_format,
        100000.0f, // max width (large enough)
        100000.0f, // max height
        &text_layout);

    if (SUCCEEDED(hr) && text_layout) {
        DWRITE_TEXT_METRICS metrics = {};
        hr = text_layout->GetMetrics(&metrics);
        if (SUCCEEDED(hr)) {
            if (out_width)  *out_width  = metrics.width;
            if (out_height) *out_height = metrics.height;
        }
        text_layout->Release();
    }

    text_format->Release();
}

void pho_platform_log(const char* message) {
    if (!message) return;
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
    fprintf(stderr, "%s\n", message);
}

char* pho_platform_clipboard_get(void) {
    if (!OpenClipboard(nullptr)) return nullptr;

    HANDLE hdata = GetClipboardData(CF_UNICODETEXT);
    if (!hdata) {
        CloseClipboard();
        return nullptr;
    }

    const wchar_t* wtext = static_cast<const wchar_t*>(GlobalLock(hdata));
    if (!wtext) {
        CloseClipboard();
        return nullptr;
    }

    std::string utf8 = utf16_to_utf8(wtext);
    GlobalUnlock(hdata);
    CloseClipboard();

    char* result = static_cast<char*>(malloc(utf8.size() + 1));
    if (result) {
        memcpy(result, utf8.c_str(), utf8.size() + 1);
    }
    return result;
}

void pho_platform_clipboard_set(const char* text) {
    if (!text) return;

    std::wstring wtext = utf8_to_utf16(text);
    size_t byte_count = (wtext.size() + 1) * sizeof(wchar_t);

    HGLOBAL hglobal = GlobalAlloc(GMEM_MOVEABLE, byte_count);
    if (!hglobal) return;

    wchar_t* dest = static_cast<wchar_t*>(GlobalLock(hglobal));
    if (!dest) {
        GlobalFree(hglobal);
        return;
    }
    memcpy(dest, wtext.c_str(), byte_count);
    GlobalUnlock(hglobal);

    if (!OpenClipboard(nullptr)) {
        GlobalFree(hglobal);
        return;
    }

    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, hglobal);
    CloseClipboard();
    // After SetClipboardData succeeds, the system owns the memory; do not free.
}

const char* pho_platform_name(void) {
    return "Windows";
}

double pho_platform_screen_scale(void) {
    // Try GetDpiForSystem (Windows 10 1607+).
    HMODULE user32 = GetModuleHandleA("user32.dll");
    if (user32) {
        typedef UINT(WINAPI* GetDpiForSystemFn)(void);
        auto fn = reinterpret_cast<GetDpiForSystemFn>(
            GetProcAddress(user32, "GetDpiForSystem"));
        if (fn) {
            UINT dpi = fn();
            if (dpi > 0) return static_cast<double>(dpi) / 96.0;
        }
    }

    // Fallback: GetDeviceCaps on the screen DC.
    HDC hdc = GetDC(nullptr);
    if (hdc) {
        int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(nullptr, hdc);
        if (dpi > 0) return static_cast<double>(dpi) / 96.0;
    }

    return 1.0;
}

int pho_platform_http_get_c(const char* url, char** out_body, size_t* out_len) {
    return winhttp_request("GET", url, nullptr, 0, nullptr, out_body, out_len);
}

int pho_platform_http_post_c(const char* url,
                             const char* body,
                             size_t body_len,
                             const char* content_type,
                             char** out_body,
                             size_t* out_len) {
    return winhttp_request("POST", url, body, body_len, content_type, out_body, out_len);
}

} // extern "C"

// ---------------------------------------------------------------------------
// C++ wrappers (namespace pho)
// ---------------------------------------------------------------------------

namespace pho {

int pho_platform_http_get(const std::string& url, std::string& out_body) {
    char* body = nullptr;
    size_t len = 0;
    int status = pho_platform_http_get_c(url.c_str(), &body, &len);
    if (body) {
        out_body.assign(body, len);
        free(body);
    } else {
        out_body.clear();
    }
    return status;
}

int pho_platform_http_post(const std::string& url,
                           const std::string& body,
                           const std::string& content_type,
                           std::string& out_body) {
    char* resp_body = nullptr;
    size_t resp_len = 0;
    int status = pho_platform_http_post_c(url.c_str(),
                                          body.data(),
                                          body.size(),
                                          content_type.c_str(),
                                          &resp_body,
                                          &resp_len);
    if (resp_body) {
        out_body.assign(resp_body, resp_len);
        free(resp_body);
    } else {
        out_body.clear();
    }
    return status;
}

} // namespace pho
