#include "stdafx.h"
#include "JsonFile.h"

HRESULT UpdateJsonFile(
    __in_z LPCWSTR wzFile,
    __in_z LPCWSTR wzElementPath,
    __in_z LPCWSTR wzValue,
    __in int iFlags,
    __in int iIndex,
    __in_z LPCWSTR wzSchemaFile
)
{
    HRESULT hr = S_OK;
    ::SetLastError(0);

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
            return S_OK;
        }

        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - File not found: '%ls'", wzFile);

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

        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    std::string elementPath;
    hr = WideToUtf8(wzElementPath, elementPath);
    if (FAILED(hr))
    {
        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Failed to convert element path '%ls' to UTF-8 for file '%ls' (hr=0x%08X)", wzElementPath, wzFile, static_cast<unsigned int>(hr));
        return hr;
    }

    WcaLog(LOGMSG_VERBOSE, "Element path: %ls", wzElementPath);

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
                    return E_FAIL;
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
                    return S_OK; // Skip the operation but return success
                }
            }
            else
            {
                WcaLog(LOGMSG_STANDARD, "WixJsonFile: Failed to open file for OnlyIfExists check: %ls", wzFile);
                return HRESULT_FROM_WIN32(ERROR_OPEN_FAILED);
            }
        }
        catch (const std::exception& e)
        {
            WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error checking path existence for OnlyIfExists: %s", e.what());
            return E_FAIL;
        }
        catch (...)
        {
            WcaLog(LOGMSG_STANDARD, "WixJsonFile: Unknown error checking path existence for OnlyIfExists");
            return E_FAIL;
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

    return hr;
}
