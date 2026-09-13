#include "stdafx.h"
#include "JsonFile.h"

const char* JsonActionName(__in int iFlags)
{
    std::bitset<32> flags(iFlags);
    if (flags.test(FLAG_CREATEVALUE)) return "createJsonPointerValue";
    if (flags.test(FLAG_SETVALUE)) return "setValue";
    if (flags.test(FLAG_DELETEVALUE)) return "deleteValue";
    if (flags.test(FLAG_REPLACEJSONVALUE)) return "replaceJsonValue";
    if (flags.test(FLAG_APPENDARRAY)) return "appendArray";
    if (flags.test(FLAG_INSERTARRAY)) return "insertArray";
    if (flags.test(FLAG_REMOVEARRAYELEMENT)) return "removeArrayElement";
    if (flags.test(FLAG_DISTINCTVALUES)) return "distinctValues";
    if (flags.test(FLAG_READVALUE)) return "readValue";
    return "unknown";
}

std::string DescribeJsonAtPath(__in_z LPCWSTR wzFile, const std::string& elementPath, bool fPointer)
{
    const size_t cchMax = 2048;
    try
    {
        if (NULL == wzFile || L'\0' == *wzFile || !fs::exists(fs::path(wzFile)))
        {
            return "<file not found>";
        }

        std::ifstream is{ fs::path(wzFile) };
        if (!is.is_open())
        {
            return "<file not readable>";
        }

        json j;
        try
        {
            j = json::parse(is);
        }
        catch (const std::exception&)
        {
            return "<not valid JSON>";
        }

        std::string text;
        if (fPointer)
        {
            std::error_code ec;
            const json& value = jsonpointer::get(j, elementPath, ec);
            if (ec)
            {
                return "<no match>";
            }
            text = value.to_string();
        }
        else
        {
            json matches = jsonpath::json_query(j, elementPath);
            if (matches.empty())
            {
                return "<no match>";
            }
            text = matches.to_string();
        }

        if (text.size() > cchMax)
        {
            text.resize(cchMax);
            text += "...";
        }
        return text;
    }
    catch (const std::exception& e)
    {
        return std::string("<error: ") + e.what() + ">";
    }
    catch (...)
    {
        return "<error>";
    }
}

// Records the outcome on the trace (when one was requested) and returns hr unchanged, so every
// exit path of UpdateJsonFile can be written as "return Finish(...)".
static HRESULT FinishOperation(__inout_opt JSON_OPERATION_TRACE* pTrace, const char* szOutcome, HRESULT hr,
    __in_z LPCWSTR wzFile, const std::string& elementPath, bool fPointer, bool fCaptureAfter)
{
    if (pTrace)
    {
        pTrace->outcome = szOutcome;
        if (fCaptureAfter)
        {
            pTrace->after = DescribeJsonAtPath(wzFile, elementPath, fPointer);
        }
    }
    return hr;
}

HRESULT UpdateJsonFile(
    __in_z LPCWSTR wzFile,
    __in_z LPCWSTR wzElementPath,
    __in_z LPCWSTR wzValue,
    __in int iFlags,
    __in int iIndex,
    __in_z LPCWSTR wzSchemaFile,
    __in int iOptions,
    __inout_opt JSON_OPERATION_TRACE* pTrace
)
{
    HRESULT hr = S_OK;
    ::SetLastError(0);

    // JSON_OPTION_VERBOSE is set by the scheduler for JSONEXT_LOGLEVEL=verbose and whenever the
    // MSI log itself is verbose (deferred actions cannot see MsiLogging themselves).
    const bool fVerbose = 0 != (iOptions & JSON_OPTION_VERBOSE);
    const bool fDryRun = 0 != (iOptions & JSON_OPTION_DRYRUN);

    // Input validation
    if (NULL == wzFile || L'\0' == *wzFile)
    {
        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Invalid file path parameter");
        return E_INVALIDARG;
    }

    if (NULL == wzElementPath || L'\0' == *wzElementPath)
    {
        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Invalid element path parameter for file '%ls'", wzFile);
        return E_INVALIDARG;
    }


    std::bitset<32> flags(iFlags);
    WcaLog(LOGMSG_VERBOSE, "WixJsonFile: Processing file '%ls' with flags: %i", wzFile, iFlags);

    const bool fPointer = flags.test(FLAG_CREATEVALUE);
    const char* szAction = JsonActionName(iFlags);

    // Check if OnlyIfExists flag is set
    bool onlyIfExists = flags.test(FLAG_ONLYIFEXISTS);

    // OnlyIfExists applies to every write action: skip the operation unless the target path
    // already exists. createJsonPointerValue uses JSON Pointer syntax; all other actions use JSONPath.
    bool isWriteAction = flags.test(FLAG_SETVALUE) || flags.test(FLAG_CREATEVALUE) || flags.test(FLAG_REPLACEJSONVALUE) ||
                         flags.test(FLAG_DELETEVALUE) || flags.test(FLAG_APPENDARRAY) || flags.test(FLAG_INSERTARRAY) ||
                         flags.test(FLAG_REMOVEARRAYELEMENT) || flags.test(FLAG_DISTINCTVALUES);

    // Check if file exists before attempting to parse
    if (!fs::exists(fs::path(wzFile)))
    {
        // A missing file trivially means the target path does not exist, so OnlyIfExists
        // turns this into a successful no-op instead of a failed install.
        if (onlyIfExists && isWriteAction)
        {
            WcaLog(LOGMSG_STANDARD, "WixJsonFile: Skipping operation - file does not exist and OnlyIfExists=yes: '%ls'", wzFile);
            if (pTrace) { pTrace->before = "<file not found>"; }
            return FinishOperation(pTrace, "skipped", S_OK, wzFile, "", fPointer, false);
        }

        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - File not found: '%ls'", wzFile);
        if (pTrace) { pTrace->before = "<file not found>"; }

        // Additional diagnostics (verbose so a failing install log is not flooded)
        fs::path filePath(wzFile);
        fs::path parentDir = filePath.parent_path();
        if (!parentDir.empty())
        {
            if (fs::exists(parentDir))
            {
                WcaLog(LOGMSG_VERBOSE, "WixJsonFile: Parent directory exists: '%ls'", parentDir.wstring().c_str());
                // List files in the directory to help diagnose
                try
                {
                    WcaLog(LOGMSG_VERBOSE, "WixJsonFile: Files in directory:");
                    int fileCount = 0;
                    for (const auto& entry : fs::directory_iterator(parentDir))
                    {
                        if (fileCount < 10) // Limit to first 10 files
                        {
                            WcaLog(LOGMSG_VERBOSE, "WixJsonFile:   - %ls", entry.path().filename().wstring().c_str());
                        }
                        fileCount++;
                    }
                    if (fileCount > 10)
                    {
                        WcaLog(LOGMSG_VERBOSE, "WixJsonFile:   ... and %d more files", fileCount - 10);
                    }
                    else if (fileCount == 0)
                    {
                        WcaLog(LOGMSG_VERBOSE, "WixJsonFile:   (directory is empty)");
                    }
                }
                catch (...)
                {
                    WcaLog(LOGMSG_VERBOSE, "WixJsonFile: Could not list directory contents");
                }
            }
            else
            {
                WcaLog(LOGMSG_STANDARD, "WixJsonFile: Parent directory does NOT exist: '%ls'", parentDir.wstring().c_str());
            }
        }

        return FinishOperation(pTrace, "failed", HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), wzFile, "", fPointer, false);
    }

    std::string elementPath;
    hr = WideToUtf8(wzElementPath, elementPath);
    if (FAILED(hr))
    {
        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Failed to convert element path '%ls' to UTF-8 for file '%ls' (hr=0x%08X)", wzElementPath, wzFile, static_cast<unsigned int>(hr));
        return FinishOperation(pTrace, "failed", hr, wzFile, "", fPointer, false);
    }

    WcaLog(LOGMSG_VERBOSE, "Element path: %ls", wzElementPath);

    // Diagnostics: what is at the path right now. Captured whenever the caller wants a trace
    // (transform log) or verbose logging is on; it is an extra parse of the file, so not otherwise.
    if (pTrace || fVerbose)
    {
        std::string before = DescribeJsonAtPath(wzFile, elementPath, fPointer);
        if (fVerbose)
        {
            std::string pathUtf8, fileUtf8;
            WideToUtf8(wzElementPath, pathUtf8);
            WideToUtf8(wzFile, fileUtf8);
            JsonLogRaw(std::string("WixJsonFile: ") + szAction + " '" + pathUtf8 + "' in '" + fileUtf8 + "' - before: " + before);
        }
        if (pTrace) { pTrace->before = before; }
    }

    // Dry run: report the operation and stop before anything (including OnlyIfExists probing of
    // the file, which is harmless but pointless) touches the disk.
    if (fDryRun)
    {
        WcaLog(LOGMSG_STANDARD, "WixJsonFile: DRY RUN - would apply %s to '%ls' in '%ls'%s%ls%s (flags=%d, index=%d)",
            szAction, wzElementPath, wzFile,
            (wzValue && *wzValue) ? " with value '" : "", (wzValue && *wzValue) ? wzValue : L"", (wzValue && *wzValue) ? "'" : "",
            iFlags, iIndex);
        return FinishOperation(pTrace, "dry-run", S_OK, wzFile, elementPath, fPointer, false);
    }

    if (onlyIfExists && isWriteAction)
    {
        try
        {
            json j;
            std::ifstream is{ fs::path(wzFile) };
            if (is.is_open())
            {
                try {
                    is >> j;
                }
                catch (const std::exception& e) {
                    is.close();
                    WcaLog(LOGMSG_STANDARD, "WixJsonFile: Failed to parse JSON file for OnlyIfExists check: %ls. Error: %s", wzFile, e.what());
                    return FinishOperation(pTrace, "failed", E_FAIL, wzFile, elementPath, fPointer, false);
                }
                is.close();
                
                // Check whether the target path exists using the syntax appropriate to the action.
                bool pathExists;
                if (flags.test(FLAG_CREATEVALUE))
                {
                    pathExists = jsonpointer::contains(j, elementPath);
                }
                else
                {
                    pathExists = !jsonpath::json_query(j, elementPath).empty();
                }

                if (!pathExists)
                {
                    WcaLog(LOGMSG_STANDARD, "WixJsonFile: Skipping operation - path does not exist and OnlyIfExists=yes: '%ls'", wzElementPath);
                    return FinishOperation(pTrace, "skipped", S_OK, wzFile, elementPath, fPointer, false); // Skip the operation but return success
                }
            }
            else
            {
                WcaLog(LOGMSG_STANDARD, "WixJsonFile: Failed to open file for OnlyIfExists check: %ls", wzFile);
                return FinishOperation(pTrace, "failed", HRESULT_FROM_WIN32(ERROR_OPEN_FAILED), wzFile, elementPath, fPointer, false);
            }
        }
        catch (const std::exception& e)
        {
            WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error checking path existence for OnlyIfExists: %s", e.what());
            return FinishOperation(pTrace, "failed", E_FAIL, wzFile, elementPath, fPointer, false);
        }
        catch (...)
        {
            WcaLog(LOGMSG_STANDARD, "WixJsonFile: Unknown error checking path existence for OnlyIfExists");
            return FinishOperation(pTrace, "failed", E_FAIL, wzFile, elementPath, fPointer, false);
        }
    }

    bool create = flags.test(FLAG_CREATEVALUE);
    if (flags.test(FLAG_SETVALUE) || create) {
        WcaLog(LOGMSG_VERBOSE, "Setting JSON value (create=%s)", create ? "true" : "false");
        hr = SetJsonPathValue(wzFile, elementPath, wzValue, create);
    }
    else if (flags.test(FLAG_DELETEVALUE)) {
        WcaLog(LOGMSG_VERBOSE, "Deleting JSON value");
        hr = DeleteJsonPath(wzFile, elementPath);
    }
    else if (flags.test(FLAG_REPLACEJSONVALUE)) {
        WcaLog(LOGMSG_VERBOSE, "Replacing JSON object");
        hr = SetJsonPathObject(wzFile, elementPath, wzValue);
    }
    else if (flags.test(FLAG_APPENDARRAY)) {
        WcaLog(LOGMSG_VERBOSE, "Appending to JSON array");
        hr = AppendJsonArray(wzFile, elementPath, wzValue);
    }
    else if (flags.test(FLAG_INSERTARRAY)) {
        WcaLog(LOGMSG_VERBOSE, "Inserting into JSON array at index %d", iIndex);
        hr = InsertJsonArray(wzFile, elementPath, wzValue, iIndex);
    }
    else if (flags.test(FLAG_REMOVEARRAYELEMENT)) {
        WcaLog(LOGMSG_VERBOSE, "Removing element from JSON array");
        hr = RemoveJsonArrayElement(wzFile, elementPath, wzValue);
    }
    else if (flags.test(FLAG_DISTINCTVALUES)) {
        WcaLog(LOGMSG_VERBOSE, "Removing duplicates from JSON array");
        hr = DistinctJsonArray(wzFile, elementPath);
    }

    // Validate against schema if specified and if the operation succeeded
    if (SUCCEEDED(hr) && flags.test(FLAG_VALIDATESCHEMA) && wzSchemaFile != NULL && L'\0' != *wzSchemaFile)
    {
        WcaLog(LOGMSG_VERBOSE, "Validating JSON against schema: %ls", wzSchemaFile);
        hr = ValidateJsonSchema(wzFile, wzSchemaFile);
        if (FAILED(hr))
        {
            WcaLog(LOGMSG_STANDARD, "Schema validation failed");
        }
    }

    if (pTrace || fVerbose)
    {
        std::string after = DescribeJsonAtPath(wzFile, elementPath, fPointer);
        if (fVerbose)
        {
            char szHr[16];
            ::StringCchPrintfA(szHr, std::size(szHr), "0x%08X", static_cast<unsigned int>(hr));
            std::string pathUtf8, fileUtf8;
            WideToUtf8(wzElementPath, pathUtf8);
            WideToUtf8(wzFile, fileUtf8);
            JsonLogRaw(std::string("WixJsonFile: ") + szAction + " '" + pathUtf8 + "' in '" + fileUtf8 + "' - after: " + after + " (hr=" + szHr + ")");
        }
        if (pTrace) { pTrace->after = after; }
    }

    return FinishOperation(pTrace, SUCCEEDED(hr) ? "applied" : "failed", hr, wzFile, elementPath, fPointer, false);
}
