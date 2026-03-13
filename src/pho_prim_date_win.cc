// Windows-compatible replacement for pho_prim_date.cc
// Replaces localtime_r and strptime with Windows equivalents.

#include "pho_prim.h"
#include <ctime>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace pho {

static struct tm* localtime_safe(const time_t* timep, struct tm* result) {
    if (localtime_s(result, timep) == 0) return result;
    return nullptr;
}

// Minimal strptime supporting %Y %m %d %H %M %S
static const char* strptime_win(const char* s, const char* fmt, struct tm* tm) {
    const char* sp = s;
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            int val = 0;
            int digits = 0;
            int maxdig = (*fmt == 'Y') ? 4 : 2;
            switch (*fmt) {
                case 'Y':
                    while (digits < maxdig && *sp >= '0' && *sp <= '9') { val = val * 10 + (*sp++ - '0'); digits++; }
                    if (digits == 0) return nullptr;
                    tm->tm_year = val - 1900;
                    break;
                case 'm':
                    while (digits < maxdig && *sp >= '0' && *sp <= '9') { val = val * 10 + (*sp++ - '0'); digits++; }
                    if (digits == 0) return nullptr;
                    tm->tm_mon = val - 1;
                    break;
                case 'd':
                    while (digits < maxdig && *sp >= '0' && *sp <= '9') { val = val * 10 + (*sp++ - '0'); digits++; }
                    if (digits == 0) return nullptr;
                    tm->tm_mday = val;
                    break;
                case 'H':
                    while (digits < maxdig && *sp >= '0' && *sp <= '9') { val = val * 10 + (*sp++ - '0'); digits++; }
                    if (digits == 0) return nullptr;
                    tm->tm_hour = val;
                    break;
                case 'M':
                    while (digits < maxdig && *sp >= '0' && *sp <= '9') { val = val * 10 + (*sp++ - '0'); digits++; }
                    if (digits == 0) return nullptr;
                    tm->tm_min = val;
                    break;
                case 'S':
                    while (digits < maxdig && *sp >= '0' && *sp <= '9') { val = val * 10 + (*sp++ - '0'); digits++; }
                    if (digits == 0) return nullptr;
                    tm->tm_sec = val;
                    break;
                default:
                    return nullptr;
            }
            fmt++;
        } else {
            if (*sp != *fmt) return nullptr;
            sp++;
            fmt++;
        }
    }
    return sp;
}

void register_date_prims() {
    auto& reg = PrimitiveRegistry::instance();

    reg.register_prim("date-now", 0, 1, [](const std::vector<Value>& in) -> PrimResult {
        auto now = std::chrono::system_clock::now();
        auto epoch = now.time_since_epoch();
        double secs = std::chrono::duration<double>(epoch).count();
        return PrimResult::success(Value::date(secs));
    });

    reg.register_prim("date-create", 6, 1, [](const std::vector<Value>& in) -> PrimResult {
        struct tm t = {};
        t.tm_year = static_cast<int>(in[0].as_number()) - 1900;
        t.tm_mon = static_cast<int>(in[1].as_number()) - 1;
        t.tm_mday = static_cast<int>(in[2].as_number());
        t.tm_hour = in.size() > 3 ? static_cast<int>(in[3].as_number()) : 0;
        t.tm_min = in.size() > 4 ? static_cast<int>(in[4].as_number()) : 0;
        t.tm_sec = in.size() > 5 ? static_cast<int>(in[5].as_number()) : 0;
        t.tm_isdst = -1;
        time_t tt = mktime(&t);
        return PrimResult::success(Value::date(static_cast<double>(tt)));
    });

    reg.register_prim("date-components", 1, 6, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_date()) return PrimResult::fail();
        time_t tt = static_cast<time_t>(in[0].as_date());
        struct tm t;
        if (!localtime_safe(&tt, &t)) return PrimResult::fail();
        return PrimResult::success({
            Value::integer(t.tm_year + 1900),
            Value::integer(t.tm_mon + 1),
            Value::integer(t.tm_mday),
            Value::integer(t.tm_hour),
            Value::integer(t.tm_min),
            Value::integer(t.tm_sec)
        });
    });

    reg.register_prim("date-add", 2, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_date()) return PrimResult::fail();
        double ts = in[0].as_date() + in[1].as_number();
        return PrimResult::success(Value::date(ts));
    });

    reg.register_prim("date-diff", 2, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_date() || !in[1].is_date()) return PrimResult::fail();
        return PrimResult::success(Value::real(in[0].as_date() - in[1].as_date()));
    });

    reg.register_prim("date-format", 2, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_date() || !in[1].is_string()) return PrimResult::fail();
        time_t tt = static_cast<time_t>(in[0].as_date());
        struct tm t;
        if (!localtime_safe(&tt, &t)) return PrimResult::fail();
        char buf[256];
        strftime(buf, sizeof(buf), in[1].as_string()->c_str(), &t);
        return PrimResult::success(Value::string(buf));
    });

    reg.register_prim("date-parse", 2, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_string() || !in[1].is_string()) return PrimResult::fail();
        struct tm t = {};
        if (strptime_win(in[0].as_string()->c_str(), in[1].as_string()->c_str(), &t) == nullptr) {
            return PrimResult::fail();
        }
        t.tm_isdst = -1;
        time_t tt = mktime(&t);
        return PrimResult::success(Value::date(static_cast<double>(tt)));
    });

    reg.register_prim("date-weekday", 1, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_date()) return PrimResult::fail();
        time_t tt = static_cast<time_t>(in[0].as_date());
        struct tm t;
        if (!localtime_safe(&tt, &t)) return PrimResult::fail();
        return PrimResult::success(Value::integer(t.tm_wday));
    });

    reg.register_prim("date-compare", 2, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_date() || !in[1].is_date()) return PrimResult::fail();
        double a = in[0].as_date(), b = in[1].as_date();
        return PrimResult::success(Value::integer(a < b ? -1 : (a > b ? 1 : 0)));
    });

    reg.register_prim("date-from-timestamp", 1, 1, [](const std::vector<Value>& in) -> PrimResult {
        return PrimResult::success(Value::date(in[0].as_number()));
    });

    reg.register_prim("date-to-timestamp", 1, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_date()) return PrimResult::fail();
        return PrimResult::success(Value::real(in[0].as_date()));
    });

    // Duration helpers
    reg.register_prim("seconds", 1, 1, [](const std::vector<Value>& in) -> PrimResult {
        return PrimResult::success(Value::real(in[0].as_number()));
    });

    reg.register_prim("minutes", 1, 1, [](const std::vector<Value>& in) -> PrimResult {
        return PrimResult::success(Value::real(in[0].as_number() * 60.0));
    });

    reg.register_prim("hours", 1, 1, [](const std::vector<Value>& in) -> PrimResult {
        return PrimResult::success(Value::real(in[0].as_number() * 3600.0));
    });

    reg.register_prim("days", 1, 1, [](const std::vector<Value>& in) -> PrimResult {
        return PrimResult::success(Value::real(in[0].as_number() * 86400.0));
    });
}

} // namespace pho
