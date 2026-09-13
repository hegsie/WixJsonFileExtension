// jsoncli - command-line harness for the WixJsonFileExtension JSON operations.
//
// Runs the same transform code the deferred custom action runs, against a JSON file on disk,
// outside of any MSI. Use it to check an ElementPath, preview what an action does to a sample
// configuration file, or reproduce a failing operation from an install log. Values are taken
// literally: nothing is MSI-formatted here, so pass the already-expanded value ([PROPERTY]
// references are not resolved) and unescaped brackets (write $.a[0], not $.a[\[]0[\]]).
//
// The custom action's own WcaLog messages are not visible outside an MSI session; the harness
// prints the before/after snapshot and the HRESULT of the operation instead.

#include "stdafx.h"
#include "JsonFile.h"

#include <cstdio>
#include <cwchar>
#include <string>

static int Usage()
{
    std::fputws(
        L"Usage:\n"
        L"  jsoncli <action> <jsonFile> <elementPath> [value] [options]\n"
        L"  jsoncli readValue <jsonFile> <elementPath> [--default <value>]\n"
        L"  jsoncli validateSchema <jsonFile> <schemaFile>\n"
        L"\n"
        L"Actions (as authored in JsonFile/@Action):\n"
        L"  setValue, createJsonPointerValue, replaceJsonValue, deleteValue,\n"
        L"  appendArray, insertArray, removeArrayElement, distinctValues, readValue, validateSchema\n"
        L"\n"
        L"Options:\n"
        L"  --type <t>         ValueType: auto (default), string, number, boolean, null, json, date\n"
        L"  --culture <name>   culture for --type number/date, e.g. de-DE (default: invariant)\n"
        L"  --index <n>        insertArray position (-1 appends)\n"
        L"  --schema <file>    validate the file against this JSON schema after the operation\n"
        L"  --only-if-exists   skip the operation when elementPath does not exist (OnlyIfExists=\"yes\")\n"
        L"  --dry-run          report what would be done, write nothing (JSONEXT_DRYRUN=1)\n"
        L"  --quiet            print nothing but errors; rely on the exit code\n"
        L"\n"
        L"Exit code 0 on success, 1 when the operation fails, 2 for a usage error.\n"
        L"ElementPath is a JSONPath, except for createJsonPointerValue which takes a JSON Pointer.\n",
        stdout);
    return 2;
}

static bool ActionFlags(const std::wstring& action, int& flags)
{
    struct { const wchar_t* name; int flag; } table[] =
    {
        { L"setValue", 1 << FLAG_SETVALUE },
        { L"createJsonPointerValue", 1 << FLAG_CREATEVALUE },
        { L"replaceJsonValue", 1 << FLAG_REPLACEJSONVALUE },
        { L"deleteValue", 1 << FLAG_DELETEVALUE },
        { L"appendArray", 1 << FLAG_APPENDARRAY },
        { L"insertArray", 1 << FLAG_INSERTARRAY },
        { L"removeArrayElement", 1 << FLAG_REMOVEARRAYELEMENT },
        { L"distinctValues", 1 << FLAG_DISTINCTVALUES },
    };
    for (const auto& entry : table)
    {
        if (action == entry.name)
        {
            flags = entry.flag;
            return true;
        }
    }
    return false;
}

static int ReadValue(const std::wstring& file, const std::wstring& elementPath, const std::wstring& defaultValue, bool quiet)
{
    try
    {
        std::string pathUtf8;
        if (FAILED(WideToUtf8(elementPath.c_str(), pathUtf8)))
        {
            std::fwprintf(stderr, L"error: element path is not valid UTF-16\n");
            return 1;
        }

        if (!fs::exists(fs::path(file)))
        {
            if (!quiet) std::fwprintf(stderr, L"file not found, default applies: %ls\n", file.c_str());
            std::fwprintf(stdout, L"%ls\n", defaultValue.c_str());
            return 0;
        }

        std::ifstream is{ fs::path(file) };
        json j = json::parse(is);
        json result = jsonpath::json_query(j, pathUtf8);
        if (result.empty())
        {
            if (!quiet) std::fwprintf(stderr, L"no match, default applies\n");
            std::fwprintf(stdout, L"%ls\n", defaultValue.c_str());
            return 0;
        }

        json match = (result.is_array() && !result.empty()) ? result.at(0) : result;
        std::string valueUtf8 = match.is_string() ? match.as<std::string>() : match.to_string();
        std::wstring value;
        if (FAILED(Utf8ToWide(valueUtf8.c_str(), value)))
        {
            std::fwprintf(stderr, L"error: value is not valid UTF-8\n");
            return 1;
        }
        std::fwprintf(stdout, L"%ls\n", value.c_str());
        return 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
}

int wmain(int argc, wchar_t* argv[])
{
    if (argc < 3)
    {
        return Usage();
    }

    std::wstring action = argv[1];
    std::wstring file = argv[2];
    std::wstring elementPath;
    std::wstring value;
    std::wstring schema;
    std::wstring defaultValue;
    std::wstring culture;
    int valueType = jvtAuto;
    int index = -1;
    int options = 0;
    int flags = 0;
    bool onlyIfExists = false;
    bool quiet = false;

    if (action == L"--help" || action == L"-h" || action == L"/?")
    {
        return Usage();
    }

    if (action == L"validateSchema")
    {
        if (argc < 4) return Usage();
        schema = argv[3];
        HRESULT hr = ValidateJsonSchema(file.c_str(), schema.c_str());
        std::fwprintf(SUCCEEDED(hr) ? stdout : stderr, L"validateSchema %ls against %ls: %ls (hr=0x%08X)\n",
            file.c_str(), schema.c_str(), SUCCEEDED(hr) ? L"valid" : L"INVALID", static_cast<unsigned int>(hr));
        return SUCCEEDED(hr) ? 0 : 1;
    }

    if (argc < 4) return Usage();
    elementPath = argv[3];

    int next = 4;
    if (next < argc && std::wcsncmp(argv[next], L"--", 2) != 0)
    {
        value = argv[next++];
    }

    for (; next < argc; ++next)
    {
        std::wstring opt = argv[next];
        if (opt == L"--index" && next + 1 < argc) { index = _wtoi(argv[++next]); }
        else if (opt == L"--type" && next + 1 < argc)
        {
            std::wstring t = argv[++next];
            const wchar_t* names[] = { L"auto", L"string", L"number", L"boolean", L"null", L"json", L"date" };
            valueType = -1;
            for (int i = 0; i < 7; ++i) { if (t == names[i]) valueType = i; }
            if (valueType < 0) { std::fwprintf(stderr, L"unknown --type: %ls\n", t.c_str()); return Usage(); }
        }
        else if (opt == L"--culture" && next + 1 < argc) { culture = argv[++next]; }
        else if (opt == L"--schema" && next + 1 < argc) { schema = argv[++next]; }
        else if (opt == L"--default" && next + 1 < argc) { defaultValue = argv[++next]; }
        else if (opt == L"--only-if-exists") { onlyIfExists = true; }
        else if (opt == L"--dry-run") { options |= JSON_OPTION_DRYRUN; }
        else if (opt == L"--quiet") { quiet = true; }
        else
        {
            std::fwprintf(stderr, L"unknown option: %ls\n", opt.c_str());
            return Usage();
        }
    }

    if (action == L"readValue")
    {
        return ReadValue(file, elementPath, defaultValue, quiet);
    }

    if (!ActionFlags(action, flags))
    {
        std::fwprintf(stderr, L"unknown action: %ls\n", action.c_str());
        return Usage();
    }
    if (onlyIfExists) flags |= 1 << FLAG_ONLYIFEXISTS;
    if (!schema.empty()) flags |= 1 << FLAG_VALIDATESCHEMA;

    JSON_OPERATION_TRACE trace;
    HRESULT hr = UpdateJsonFile(file.c_str(), elementPath.c_str(), value.c_str(), flags, index, schema.c_str(), options, &trace,
                                valueType, culture.empty() ? NULL : culture.c_str());

    if (!quiet)
    {
        std::fwprintf(stdout, L"%ls %ls in %ls\n", action.c_str(), elementPath.c_str(), file.c_str());
        std::fprintf(stdout, "  before:  %s\n", trace.before.c_str());
        if (!trace.after.empty())
        {
            std::fprintf(stdout, "  after:   %s\n", trace.after.c_str());
        }
        std::fprintf(stdout, "  outcome: %s (hr=0x%08X)\n", trace.outcome.c_str(), static_cast<unsigned int>(hr));
    }
    else if (FAILED(hr))
    {
        std::fwprintf(stderr, L"%ls failed (hr=0x%08X)\n", action.c_str(), static_cast<unsigned int>(hr));
    }

    return SUCCEEDED(hr) ? 0 : 1;
}
