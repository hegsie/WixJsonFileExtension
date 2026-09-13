#include "stdafx.h"
#include "JsonFile.h"
#include <iterator>

// Not defined when _WIN32_WINNT targets pre-Vista (see targetver.h); the flag is simply
// ignored by ReplaceFileW on systems that do not support it.
#ifndef REPLACEFILE_IGNORE_ACL_ERRORS
#define REPLACEFILE_IGNORE_ACL_ERRORS 0x00000004
#endif

// The formatting conventions of an existing file, so a rewrite changes only the values and not
// every line of a source-controlled configuration file: the indentation unit (tabs, or a number
// of spaces), the line ending, and whether the file ends with a newline. Defaults (4 spaces,
// CRLF, no trailing newline) match what was always written when nothing can be detected.
JSON_FILE_FORMAT DetectJsonFileFormat(__in_z LPCWSTR wzFile)
{
    JSON_FILE_FORMAT format;
    format.indent = "    ";
    format.crlf = true;
    format.trailingNewline = false;

    try
    {
        if (NULL == wzFile || L'\0' == *wzFile || !fs::exists(fs::path(wzFile)))
        {
            return format;
        }

        std::ifstream is(fs::path(wzFile), std::ios::binary);
        std::string text((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
        if (text.empty())
        {
            return format;
        }

        size_t nl = text.find('\n');
        if (nl != std::string::npos)
        {
            format.crlf = nl > 0 && text[nl - 1] == '\r';
        }
        format.trailingNewline = text.back() == '\n';

        // The first indented line gives the indentation unit (a top-level member sits one level in).
        size_t pos = 0;
        while (pos < text.size())
        {
            size_t end = text.find('\n', pos);
            if (end == std::string::npos) end = text.size();
            size_t ws = pos;
            while (ws < end && (text[ws] == ' ' || text[ws] == '\t')) ++ws;
            if (ws > pos && ws < end && text[ws] != '\r')
            {
                format.indent = text.substr(pos, ws - pos);
                break;
            }
            pos = end + 1;
        }
    }
    catch (...)
    {
        // Fall back to the defaults.
    }
    return format;
}

std::string SerializeJson(const json& j, const JSON_FILE_FORMAT& format)
{
    const bool tabs = !format.indent.empty() && format.indent.find_first_not_of('\t') == std::string::npos;

    json_options options;
    options.indent_size(tabs ? static_cast<uint8_t>(format.indent.size()) : static_cast<uint8_t>(format.indent.empty() ? 4 : format.indent.size()));
    options.new_line_chars(format.crlf ? "\r\n" : "\n");

    std::ostringstream serialized;
    serialized << pretty_print(j, options);
    std::string text = serialized.str();

    if (tabs)
    {
        // jsoncons indents with spaces only; with indent_size = number of tabs per level, each
        // level is that many leading spaces, which map one-to-one onto tabs. Only leading spaces
        // are touched, and pretty-printed JSON never starts a line inside a string.
        std::string converted;
        converted.reserve(text.size());
        size_t pos = 0;
        while (pos < text.size())
        {
            size_t end = text.find('\n', pos);
            if (end == std::string::npos) end = text.size(); else ++end;
            size_t ws = pos;
            while (ws < end && text[ws] == ' ') ++ws;
            converted.append(ws - pos, '\t');
            converted.append(text, ws, end - ws);
            pos = end;
        }
        text.swap(converted);
    }

    if (format.trailingNewline)
    {
        text += format.crlf ? "\r\n" : "\n";
    }
    return text;
}

// Serializes the document and atomically replaces the target file: the JSON is written to a
// temporary file in the same directory, flushed, then swapped in with ReplaceFileW (which
// preserves the original file's attributes and ACLs). The original file is never truncated
// before the new content is safely on disk, so a serialization or write failure - or a crash
// mid-write - cannot corrupt the target. The existing file's formatting conventions are kept.
HRESULT WriteJsonOutput(__in_z LPCWSTR wzFile, const json& j)
{
    try
    {
        if (NULL == wzFile || L'\0' == *wzFile)
        {
            return E_INVALIDARG;
        }

        std::string serialized = SerializeJson(j, DetectJsonFileFormat(wzFile));

        fs::path targetPath(wzFile);
        fs::path tempPath = targetPath;
        tempPath += L".wixjson.tmp";

        {
            // Binary mode: the line endings were chosen above to match the existing file.
            std::ofstream os(tempPath, std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
            if (!os.is_open())
            {
                WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Failed to create temporary file for '%ls'", wzFile);
                return HRESULT_FROM_WIN32(ERROR_OPEN_FAILED);
            }

            os << serialized;
            os.close();
            if (os.fail())
            {
                WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Failed to write temporary file for '%ls'", wzFile);
                std::error_code ec;
                fs::remove(tempPath, ec);
                return HRESULT_FROM_WIN32(ERROR_WRITE_FAULT);
            }
        }

        if (!::ReplaceFileW(targetPath.c_str(), tempPath.c_str(), NULL,
                            REPLACEFILE_IGNORE_MERGE_ERRORS | REPLACEFILE_IGNORE_ACL_ERRORS, NULL, NULL))
        {
            DWORD dwError = ::GetLastError();

            // ReplaceFileW requires the target to exist; fall back to a move when it does not
            // (or when the volume rejects the replace for another transient reason).
            if (!::MoveFileExW(tempPath.c_str(), targetPath.c_str(),
                               MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                DWORD dwMoveError = ::GetLastError();
                WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Failed to replace file '%ls' (replace error=%u, move error=%u)",
                       wzFile, dwError, dwMoveError);
                std::error_code ec;
                fs::remove(tempPath, ec);
                return HRESULT_FROM_WIN32(dwMoveError ? dwMoveError : ERROR_WRITE_FAULT);
            }
        }

        return S_OK;
    }
    catch (const std::exception& e)
    {
        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Exception while writing file '%ls': %s", wzFile, e.what());
        return E_FAIL;
    }
    catch (...)
    {
        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Unknown error while writing file '%ls'", wzFile);
        return E_FAIL;
    }
}

// Parses an authored attribute value into a JSON value. Values that parse as JSON (numbers,
// booleans, null, objects, arrays, quoted strings) become that typed value; anything else is
// treated as a plain string. When the value replaces an existing string, the string type is
// preserved so values like "1.0" stay strings instead of silently becoming numbers.
json MakeJsonValue(const std::string& valueUtf8, const json* pExisting)
{
    if (pExisting != NULL && pExisting->is_string())
    {
        return json(valueUtf8);
    }

    try
    {
        return json::parse(valueUtf8);
    }
    catch (const std::exception&)
    {
        return json(valueUtf8);
    }
}
