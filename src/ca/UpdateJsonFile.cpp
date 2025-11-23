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

    _bstr_t bFile(wzFile);
    char* cFile = bFile;

    // Check if file exists before attempting to parse
    if (!fs::exists(fs::path(wzFile)))
    {
        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - File not found: '%ls'. Verify the file exists and the path is correct.", wzFile);
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    std::bitset<32> flags(iFlags);
    WcaLog(LOGMSG_VERBOSE, "WixJsonFile: Processing file '%ls' with flags: %i", wzFile, iFlags);

    _bstr_t bElementPath(wzElementPath);
    char* cElementPath = bElementPath;

    std::string elementPath(cElementPath);

    WcaLog(LOGMSG_VERBOSE, "Element path: %ls", wzElementPath);

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
