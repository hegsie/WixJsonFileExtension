#include "stdafx.h"
#include "JsonFile.h"

HRESULT UpdateJsonFile(
    __in_z LPCWSTR wzFile,
    __in_z LPCWSTR wzElementPath,
    __in_z LPCWSTR wzValue,
    __in int iFlags
)
{
    HRESULT hr = S_OK;
    ::SetLastError(0);

    // Input validation
    if (NULL == wzFile || L'\0' == *wzFile)
    {
        WcaLog(LOGMSG_STANDARD, "Invalid file path parameter");
        return E_INVALIDARG;
    }

    if (NULL == wzElementPath || L'\0' == *wzElementPath)
    {
        WcaLog(LOGMSG_STANDARD, "Invalid element path parameter");
        return E_INVALIDARG;
    }

    _bstr_t bFile(wzFile);
    char* cFile = bFile;

    // Check if file exists before attempting to parse
    if (!fs::exists(fs::path(wzFile)))
    {
        WcaLog(LOGMSG_STANDARD, "File not found: %ls", wzFile);
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    std::bitset<32> flags(iFlags);
    WcaLog(LOGMSG_VERBOSE, "Processing file with flags: %i", iFlags);

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

    return hr;
}
