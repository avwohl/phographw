// Windows-compatible replacement for pho_prim_fileio.cc
// Replaces POSIX dirent.h/unistd.h with Win32 equivalents.

#include "pho_prim.h"
#include "pho_platform.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <iomanip>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <sys/stat.h>
#include <direct.h>
#include <io.h>

namespace pho {

// Format a string with {0}, {1}, ... placeholders replaced by list elements
static std::string format_string(const std::string& tmpl, const std::vector<Value>& args) {
    std::string result;
    result.reserve(tmpl.size() * 2);
    size_t i = 0;
    while (i < tmpl.size()) {
        if (tmpl[i] == '{') {
            size_t j = i + 1;
            while (j < tmpl.size() && tmpl[j] >= '0' && tmpl[j] <= '9') j++;
            if (j < tmpl.size() && tmpl[j] == '}' && j > i + 1) {
                int idx = std::atoi(tmpl.substr(i + 1, j - i - 1).c_str());
                if (idx >= 0 && idx < (int)args.size()) {
                    result += args[idx].to_display_string();
                } else {
                    result += tmpl.substr(i, j - i + 1);
                }
                i = j + 1;
                continue;
            }
        }
        result += tmpl[i];
        i++;
    }
    return result;
}

void register_fileio_prims() {
    auto& r = PrimitiveRegistry::instance();

    // file-read-text: path -> string
    r.register_prim("file-read-text", 1, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_string()) return PrimResult::fail_with(Value::error("file-read-text: expected string path"));
        size_t len = 0;
        char* data = pho_platform_read_file(in[0].as_string()->c_str(), &len);
        if (!data) return PrimResult::fail_with(Value::error("file-read-text: could not read file"));
        std::string content(data, len);
        pho_platform_free(data);
        return PrimResult::success(Value::string(std::move(content)));
    });

    // file-write-text: path content -> boolean
    r.register_prim("file-write-text", 2, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_string() || !in[1].is_string())
            return PrimResult::fail_with(Value::error("file-write-text: expected path and content strings"));
        int rc = pho_platform_write_file(in[0].as_string()->c_str(),
                                          in[1].as_string()->c_str(),
                                          in[1].as_string()->length());
        return PrimResult::success(Value::boolean(rc == 0));
    });

    // file-read-binary: path -> data
    r.register_prim("file-read-binary", 1, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_string()) return PrimResult::fail_with(Value::error("file-read-binary: expected string path"));
        size_t len = 0;
        char* raw = pho_platform_read_file(in[0].as_string()->c_str(), &len);
        if (!raw) return PrimResult::fail_with(Value::error("file-read-binary: could not read file"));
        std::vector<uint8_t> bytes(raw, raw + len);
        pho_platform_free(raw);
        return PrimResult::success(Value::data(make_ref<PhoData>(std::move(bytes))));
    });

    // file-write-binary: path data -> boolean
    r.register_prim("file-write-binary", 2, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_string()) return PrimResult::fail_with(Value::error("file-write-binary: expected string path"));
        if (!in[1].is_data()) return PrimResult::fail_with(Value::error("file-write-binary: expected data"));
        auto* d = in[1].as_data();
        int rc = pho_platform_write_file(in[0].as_string()->c_str(), d->bytes().data(), d->length());
        return PrimResult::success(Value::boolean(rc == 0));
    });

    // file-append-text: path content -> boolean
    r.register_prim("file-append-text", 2, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_string() || !in[1].is_string())
            return PrimResult::fail_with(Value::error("file-append-text: expected path and content strings"));
        FILE* f = fopen(in[0].as_string()->c_str(), "a");
        if (!f) return PrimResult::fail_with(Value::error("file-append-text: could not open file"));
        size_t len = in[1].as_string()->length();
        size_t written = fwrite(in[1].as_string()->c_str(), 1, len, f);
        fclose(f);
        return PrimResult::success(Value::boolean(written == len));
    });

    // file-delete: path -> boolean
    r.register_prim("file-delete", 1, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_string()) return PrimResult::fail_with(Value::error("file-delete: expected string path"));
        return PrimResult::success(Value::boolean(_unlink(in[0].as_string()->c_str()) == 0));
    });

    // file-size: path -> integer
    r.register_prim("file-size", 1, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_string()) return PrimResult::fail_with(Value::error("file-size: expected string path"));
        struct _stat st;
        if (_stat(in[0].as_string()->c_str(), &st) != 0)
            return PrimResult::fail_with(Value::error("file-size: file not found"));
        return PrimResult::success(Value::integer(st.st_size));
    });

    // dir-list: path -> list of strings
    r.register_prim("dir-list", 1, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_string()) return PrimResult::fail_with(Value::error("dir-list: expected string path"));
        std::string pattern = in[0].as_string()->str();
        if (!pattern.empty() && pattern.back() != '\\' && pattern.back() != '/')
            pattern += "\\";
        pattern += "*";

        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE)
            return PrimResult::fail_with(Value::error("dir-list: could not open directory"));

        std::vector<Value> entries;
        do {
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
            entries.push_back(Value::string(fd.cFileName));
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);

        return PrimResult::success(Value::list(std::move(entries)));
    });

    // dir-create: path -> boolean
    r.register_prim("dir-create", 1, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_string()) return PrimResult::fail_with(Value::error("dir-create: expected string path"));
        return PrimResult::success(Value::boolean(_mkdir(in[0].as_string()->c_str()) == 0));
    });

    // path-join: parts (list) -> string
    r.register_prim("path-join", 1, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_list()) return PrimResult::fail_with(Value::error("path-join: expected list of strings"));
        auto* lst = in[0].as_list();
        std::string result;
        for (size_t i = 0; i < lst->size(); i++) {
            if (!lst->at(i).is_string())
                return PrimResult::fail_with(Value::error("path-join: all elements must be strings"));
            if (i > 0 && !result.empty() && result.back() != '/' && result.back() != '\\')
                result += '\\';
            result += lst->at(i).as_string()->str();
        }
        return PrimResult::success(Value::string(std::move(result)));
    });

    // path-extension: path -> string
    r.register_prim("path-extension", 1, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_string()) return PrimResult::fail_with(Value::error("path-extension: expected string"));
        const std::string& path = in[0].as_string()->str();
        auto dot = path.rfind('.');
        auto slash = path.find_last_of("/\\");
        if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
            return PrimResult::success(Value::string(""));
        return PrimResult::success(Value::string(path.substr(dot + 1)));
    });

    // path-basename: path -> string
    r.register_prim("path-basename", 1, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_string()) return PrimResult::fail_with(Value::error("path-basename: expected string"));
        const std::string& path = in[0].as_string()->str();
        auto slash = path.find_last_of("/\\");
        if (slash == std::string::npos) return PrimResult::success(in[0]);
        return PrimResult::success(Value::string(path.substr(slash + 1)));
    });

    // path-dirname: path -> string
    r.register_prim("path-dirname", 1, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_string()) return PrimResult::fail_with(Value::error("path-dirname: expected string"));
        const std::string& path = in[0].as_string()->str();
        auto slash = path.find_last_of("/\\");
        if (slash == std::string::npos) return PrimResult::success(Value::string("."));
        return PrimResult::success(Value::string(path.substr(0, slash)));
    });

    // file-is-directory?: path -> boolean
    r.register_prim("file-is-directory?", 1, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_string()) return PrimResult::fail_with(Value::error("file-is-directory?: expected string"));
        DWORD attrs = GetFileAttributesA(in[0].as_string()->c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) return PrimResult::success(Value::boolean(false));
        return PrimResult::success(Value::boolean((attrs & FILE_ATTRIBUTE_DIRECTORY) != 0));
    });

    // fmt: template args-list -> string
    r.register_prim("fmt", 2, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_string()) return PrimResult::fail_with(Value::error("fmt: expected string template"));
        if (!in[1].is_list()) return PrimResult::fail_with(Value::error("fmt: expected list of args"));
        auto* lst = in[1].as_list();
        return PrimResult::success(Value::string(format_string(in[0].as_string()->str(), lst->elems())));
    });

    // fmt-pad-left: string width char -> string
    r.register_prim("fmt-pad-left", 3, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_string() || !in[1].is_integer() || !in[2].is_string())
            return PrimResult::fail_with(Value::error("fmt-pad-left: expected string, integer, string"));
        std::string s = in[0].as_string()->str();
        int64_t width = in[1].as_integer();
        char pad = in[2].as_string()->length() > 0 ? in[2].as_string()->c_str()[0] : ' ';
        while ((int64_t)s.size() < width) s.insert(s.begin(), pad);
        return PrimResult::success(Value::string(std::move(s)));
    });

    // fmt-pad-right: string width char -> string
    r.register_prim("fmt-pad-right", 3, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_string() || !in[1].is_integer() || !in[2].is_string())
            return PrimResult::fail_with(Value::error("fmt-pad-right: expected string, integer, string"));
        std::string s = in[0].as_string()->str();
        int64_t width = in[1].as_integer();
        char pad = in[2].as_string()->length() > 0 ? in[2].as_string()->c_str()[0] : ' ';
        while ((int64_t)s.size() < width) s.push_back(pad);
        return PrimResult::success(Value::string(std::move(s)));
    });

    // fmt-number: number decimals -> string
    r.register_prim("fmt-number", 2, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_numeric() || !in[1].is_integer())
            return PrimResult::fail_with(Value::error("fmt-number: expected number, integer"));
        std::ostringstream oss;
        oss << std::fixed << std::setprecision((int)in[1].as_integer()) << in[0].as_number();
        return PrimResult::success(Value::string(oss.str()));
    });

    // fmt-join: list separator -> string
    r.register_prim("fmt-join", 2, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_list() || !in[1].is_string())
            return PrimResult::fail_with(Value::error("fmt-join: expected list and string separator"));
        auto* lst = in[0].as_list();
        const std::string& sep = in[1].as_string()->str();
        std::string result;
        for (size_t i = 0; i < lst->size(); i++) {
            if (i > 0) result += sep;
            result += lst->at(i).to_display_string();
        }
        return PrimResult::success(Value::string(std::move(result)));
    });

    // temp-dir: -> string
    r.register_prim("temp-dir", 0, 1, [](const std::vector<Value>&) -> PrimResult {
        char buf[MAX_PATH];
        DWORD len = GetTempPathA(MAX_PATH, buf);
        if (len == 0) return PrimResult::success(Value::string("C:\\Temp"));
        return PrimResult::success(Value::string(std::string(buf, len)));
    });

    // cwd: -> string
    r.register_prim("cwd", 0, 1, [](const std::vector<Value>&) -> PrimResult {
        char buf[MAX_PATH];
        if (!_getcwd(buf, sizeof(buf))) return PrimResult::fail_with(Value::error("cwd: failed"));
        return PrimResult::success(Value::string(buf));
    });

    // file-rename: old-path new-path -> boolean
    r.register_prim("file-rename", 2, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_string() || !in[1].is_string())
            return PrimResult::fail_with(Value::error("file-rename: expected two string paths"));
        return PrimResult::success(Value::boolean(
            rename(in[0].as_string()->c_str(), in[1].as_string()->c_str()) == 0));
    });
}

} // namespace pho
