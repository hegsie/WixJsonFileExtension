#include "stdafx.h"
#include "JsonFile.h"

// Serializes the document and atomically replaces the target file: the JSON is written to a
// temporary file in the same directory, flushed, then swapped in with ReplaceFileW (which
// preserves the original file's attributes and ACLs). The original file is never truncated
// before the new content is safely on disk, so a serialization or write failure - or a crash
// mid-write - cannot corrupt the target.
HRESULT WriteJsonOutput(__in_z LPCWSTR wzFile, const json& j)
{
    try
    {
        if (NULL == wzFile || L'\0' == *wzFile)
        {
            return E_INVALIDARG;
        }

        std::ostringstream serialized;
        serialized << pretty_print(j);
        if (serialized.fail())
        {
            WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Failed to serialize JSON for file '%ls'", wzFile);
            return HRESULT_FROM_WIN32(ERROR_WRITE_FAULT);
        }

        fs::path targetPath(wzFile);
        fs::path tempPath = targetPath;
        tempPath += L".wixjson.tmp";

        {
            // Text mode (like the previous in-place writes) so line endings stay CRLF on Windows.
            std::ofstream os(tempPath, std::ios_base::out | std::ios_base::trunc);
            if (!os.is_open())
            {
                WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Failed to create temporary file for '%ls'", wzFile);
                return HRESULT_FROM_WIN32(ERROR_OPEN_FAILED);
            }

            os << serialized.str();
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
