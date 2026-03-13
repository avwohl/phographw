// Windows-compatible replacement for pho_prim_locale.cc
// Replaces localtime_r/tm_gmtoff with Windows equivalents.

#include "pho_prim.h"
#include <clocale>
#include <ctime>
#include <cstring>
#include <cstdio>
#include <sstream>
#include <iomanip>
#include <cstdlib>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace pho {

void register_locale_prims() {
    auto& r = PrimitiveRegistry::instance();

    // locale-language: -> string
    r.register_prim("locale-language", 0, 1, [](const std::vector<Value>&) -> PrimResult {
        wchar_t buf[16];
        int len = GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, buf, 16);
        if (len > 0) {
            std::string lang;
            for (int i = 0; i < len - 1; i++) lang += (char)buf[i];
            return PrimResult::success(Value::string(std::move(lang)));
        }
        return PrimResult::success(Value::string("en"));
    });

    // locale-country: -> string
    r.register_prim("locale-country", 0, 1, [](const std::vector<Value>&) -> PrimResult {
        wchar_t buf[16];
        int len = GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, buf, 16);
        if (len > 0) {
            std::string country;
            for (int i = 0; i < len - 1; i++) country += (char)buf[i];
            return PrimResult::success(Value::string(std::move(country)));
        }
        return PrimResult::success(Value::string("US"));
    });

    // locale-currency-symbol: -> string
    r.register_prim("locale-currency-symbol", 0, 1, [](const std::vector<Value>&) -> PrimResult {
        wchar_t buf[16];
        int len = GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SCURRENCY, buf, 16);
        if (len > 0) {
            // Convert wchar to UTF-8
            int needed = WideCharToMultiByte(CP_UTF8, 0, buf, len - 1, nullptr, 0, nullptr, nullptr);
            std::string sym(needed, '\0');
            WideCharToMultiByte(CP_UTF8, 0, buf, len - 1, &sym[0], needed, nullptr, nullptr);
            return PrimResult::success(Value::string(std::move(sym)));
        }
        return PrimResult::success(Value::string("$"));
    });

    // locale-decimal-sep: -> string
    r.register_prim("locale-decimal-sep", 0, 1, [](const std::vector<Value>&) -> PrimResult {
        wchar_t buf[8];
        int len = GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SDECIMAL, buf, 8);
        if (len > 0) {
            std::string sep;
            for (int i = 0; i < len - 1; i++) sep += (char)buf[i];
            return PrimResult::success(Value::string(std::move(sep)));
        }
        return PrimResult::success(Value::string("."));
    });

    // locale-thousands-sep: -> string
    r.register_prim("locale-thousands-sep", 0, 1, [](const std::vector<Value>&) -> PrimResult {
        wchar_t buf[8];
        int len = GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_STHOUSAND, buf, 8);
        if (len > 0) {
            std::string sep;
            for (int i = 0; i < len - 1; i++) sep += (char)buf[i];
            return PrimResult::success(Value::string(std::move(sep)));
        }
        return PrimResult::success(Value::string(","));
    });

    // locale-timezone: -> string
    r.register_prim("locale-timezone", 0, 1, [](const std::vector<Value>&) -> PrimResult {
        TIME_ZONE_INFORMATION tzi;
        GetTimeZoneInformation(&tzi);
        // Convert StandardName (wchar) to string
        int needed = WideCharToMultiByte(CP_UTF8, 0, tzi.StandardName, -1, nullptr, 0, nullptr, nullptr);
        std::string name(needed - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, tzi.StandardName, -1, &name[0], needed, nullptr, nullptr);
        return PrimResult::success(Value::string(std::move(name)));
    });

    // locale-tz-offset: -> integer (seconds from UTC)
    r.register_prim("locale-tz-offset", 0, 1, [](const std::vector<Value>&) -> PrimResult {
        TIME_ZONE_INFORMATION tzi;
        DWORD result = GetTimeZoneInformation(&tzi);
        long bias = tzi.Bias;  // minutes west of UTC
        if (result == TIME_ZONE_ID_DAYLIGHT)
            bias += tzi.DaylightBias;
        else
            bias += tzi.StandardBias;
        return PrimResult::success(Value::integer((int64_t)(-bias * 60)));
    });

    // locale-is-dst: -> boolean
    r.register_prim("locale-is-dst", 0, 1, [](const std::vector<Value>&) -> PrimResult {
        TIME_ZONE_INFORMATION tzi;
        DWORD result = GetTimeZoneInformation(&tzi);
        return PrimResult::success(Value::boolean(result == TIME_ZONE_ID_DAYLIGHT));
    });

    // locale-date-format: -> string
    r.register_prim("locale-date-format", 0, 1, [](const std::vector<Value>&) -> PrimResult {
        return PrimResult::success(Value::string("yyyy-MM-dd"));
    });

    // locale-time-format: -> string
    r.register_prim("locale-time-format", 0, 1, [](const std::vector<Value>&) -> PrimResult {
        return PrimResult::success(Value::string("HH:mm:ss"));
    });

    // locale-format-number: number -> string
    r.register_prim("locale-format-number", 1, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_numeric()) return PrimResult::fail_with(Value::error("locale-format-number: expected number"));
        std::ostringstream oss;
        oss << in[0].as_number();
        return PrimResult::success(Value::string(oss.str()));
    });

    // locale-format-currency: number -> string
    r.register_prim("locale-format-currency", 1, 1, [](const std::vector<Value>& in) -> PrimResult {
        if (!in[0].is_numeric()) return PrimResult::fail_with(Value::error("locale-format-currency: expected number"));
        std::ostringstream oss;
        oss << "$" << std::fixed << std::setprecision(2) << in[0].as_number();
        return PrimResult::success(Value::string(oss.str()));
    });
}

} // namespace pho
