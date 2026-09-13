#include "stdafx.h"
#include "JsonFile.h"
#include <oleauto.h>
#include <charconv>
#include <cstring>
#include <cwctype>

// ValueType / Culture handling for the Value attribute.
//
// jvtAuto keeps the historical behaviour (see MakeJsonValue): the value is parsed as JSON when it
// parses, otherwise it is a string, except that an existing string stays a string. The explicit
// types make the JSON type a decision of the authoring rather than of the value's spelling, and
// number/date accept the conventions of a named culture so "1.234,5" (de-DE) or "31/12/2024"
// (en-GB) can come from a property the user typed.

static std::wstring ToWide(const std::string& utf8)
{
    std::wstring wide;
    Utf8ToWide(utf8.c_str(), wide);
    return wide;
}

static std::string ToUtf8(const std::wstring& wide)
{
    std::string utf8;
    WideToUtf8(wide.c_str(), utf8);
    return utf8;
}

static bool EqualsNoCase(const std::string& a, const char* b)
{
    return 0 == _stricmp(a.c_str(), b);
}

static std::string Trim(const std::string& s)
{
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

static bool ParseBoolean(const std::string& valueUtf8, bool& out)
{
    std::string v = Trim(valueUtf8);
    if (EqualsNoCase(v, "true") || EqualsNoCase(v, "yes") || EqualsNoCase(v, "on") || v == "1") { out = true; return true; }
    if (EqualsNoCase(v, "false") || EqualsNoCase(v, "no") || EqualsNoCase(v, "off") || v == "0") { out = false; return true; }
    return false;
}

// Normalizes a culture-formatted number to invariant form ("1.234,5" de-DE -> "1234.5") and parses
// it. Without a culture the invariant conventions apply (".", no grouping), plus a tolerance for
// surrounding whitespace.
static bool ParseNumber(const std::string& valueUtf8, LPCWSTR wzCulture, json& out, std::string& error)
{
    std::wstring text = ToWide(Trim(valueUtf8));
    std::wstring decimal = L".";
    std::wstring group;

    if (wzCulture && *wzCulture)
    {
        wchar_t wzDecimal[16] = L"";
        wchar_t wzGroup[16] = L"";
        if (0 == ::GetLocaleInfoEx(wzCulture, LOCALE_SDECIMAL, wzDecimal, static_cast<int>(std::size(wzDecimal))) ||
            0 == ::GetLocaleInfoEx(wzCulture, LOCALE_STHOUSAND, wzGroup, static_cast<int>(std::size(wzGroup))))
        {
            error = "unknown culture '" + ToUtf8(wzCulture) + "'";
            return false;
        }
        decimal = wzDecimal;
        group = wzGroup;
    }

    // Drop grouping separators (and the no-break spaces some cultures use for them), then map the
    // culture's decimal separator to '.'.
    std::wstring normalized;
    for (size_t i = 0; i < text.size();)
    {
        if (!group.empty() && text.compare(i, group.size(), group) == 0) { i += group.size(); continue; }
        if (text.compare(i, decimal.size(), decimal) == 0) { normalized += L'.'; i += decimal.size(); continue; }
        wchar_t c = text[i++];
        if (c == L' ' || c == L'\u00A0' || c == L'\u202F') continue;   // space, no-break space, narrow no-break space
        normalized += c;
    }

    std::string ascii = ToUtf8(normalized);
    if (ascii.empty())
    {
        error = "empty number";
        return false;
    }

    bool integral = ascii.find_first_of(".eE") == std::string::npos;
    if (integral)
    {
        int64_t i64 = 0;
        auto r = std::from_chars(ascii.data(), ascii.data() + ascii.size(), i64);
        if (r.ec == std::errc() && r.ptr == ascii.data() + ascii.size())
        {
            out = json(i64);
            return true;
        }
    }

    double d = 0;
    auto r = std::from_chars(ascii.data(), ascii.data() + ascii.size(), d);
    if (r.ec != std::errc() || r.ptr != ascii.data() + ascii.size())
    {
        error = "'" + valueUtf8 + "' is not a number" + (wzCulture && *wzCulture ? " in culture " + ToUtf8(wzCulture) : "");
        return false;
    }
    out = json(d);
    return true;
}

// Parses a date/time in the culture's conventions (OLE Automation rules, which also accept ISO
// 8601 dates) and writes it back as ISO 8601 "yyyy-MM-ddTHH:mm:ss" so the JSON is culture-neutral.
static bool ParseDate(const std::string& valueUtf8, LPCWSTR wzCulture, json& out, std::string& error)
{
    std::wstring text = ToWide(Trim(valueUtf8));
    LCID lcid = LOCALE_INVARIANT;
    if (wzCulture && *wzCulture)
    {
        lcid = ::LocaleNameToLCID(wzCulture, 0);
        if (0 == lcid)
        {
            error = "unknown culture '" + ToUtf8(wzCulture) + "'";
            return false;
        }
    }

    DATE date = 0;
    HRESULT hr = ::VarDateFromStr(text.c_str(), lcid, 0, &date);
    SYSTEMTIME st = {};
    if (FAILED(hr) || !::VariantTimeToSystemTime(date, &st))
    {
        error = "'" + valueUtf8 + "' is not a date" + (wzCulture && *wzCulture ? " in culture " + ToUtf8(wzCulture) : "");
        return false;
    }

    char sz[32];
    ::StringCchPrintfA(sz, std::size(sz), "%04u-%02u-%02uT%02u:%02u:%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    out = json(std::string(sz));
    return true;
}

bool ConvertAuthoredValue(const std::string& valueUtf8, int iValueType, LPCWSTR wzCulture, const json* pExisting, json& out, std::string& error)
{
    error.clear();
    switch (iValueType)
    {
        case jvtString:
            out = json(valueUtf8);
            return true;

        case jvtNumber:
            return ParseNumber(valueUtf8, wzCulture, out, error);

        case jvtBoolean:
        {
            bool b = false;
            if (!ParseBoolean(valueUtf8, b))
            {
                error = "'" + valueUtf8 + "' is not a boolean (expected true/false, yes/no, on/off or 1/0)";
                return false;
            }
            out = json(b);
            return true;
        }

        case jvtNull:
            out = json::null();
            return true;

        case jvtJson:
            try
            {
                out = json::parse(valueUtf8);
                return true;
            }
            catch (const std::exception& e)
            {
                error = std::string("value is not valid JSON: ") + e.what();
                return false;
            }

        case jvtDate:
            return ParseDate(valueUtf8, wzCulture, out, error);

        case jvtAuto:
        default:
            out = MakeJsonValue(valueUtf8, pExisting);
            return true;
    }
}

const char* JsonValueTypeName(int iValueType)
{
    switch (iValueType)
    {
        case jvtString: return "string";
        case jvtNumber: return "number";
        case jvtBoolean: return "boolean";
        case jvtNull: return "null";
        case jvtJson: return "json";
        case jvtDate: return "date";
        default: return "auto";
    }
}
