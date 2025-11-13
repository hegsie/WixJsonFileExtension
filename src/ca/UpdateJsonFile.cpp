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

    _bstr_t bFile(wzFile);
    char* cFile = bFile;

    // Check if file exists before attempting to parse
    if (!fs::exists(fs::path(wzFile)))
    {
        WcaLog(LOGMSG_STANDARD, "Unable to open the target file: %ls", wzFile);
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    std::bitset<32> flags(iFlags);
    WcaLog(LOGMSG_STANDARD, "Using the following flags, %i", iFlags);

    _bstr_t bElementPath(wzElementPath);
    char* cElementPath = bElementPath;

    std::string elementPath(cElementPath);

    WcaLog(LOGMSG_STANDARD, "Found ElementPath: %ls", wzElementPath);

    bool create = flags.test(FLAG_CREATEVALUE);
    WcaLog(LOGMSG_STANDARD, "Found create set to %s", create ? "true" : "false");
    if (flags.test(FLAG_SETVALUE) || create) {
        WcaLog(LOGMSG_STANDARD, "FLAG_SETVALUE");
        hr = SetJsonPathValue(wzFile, elementPath, wzValue, create);
    }
    else if (flags.test(FLAG_DELETEVALUE)) {
        WcaLog(LOGMSG_STANDARD, "FLAG_DELETEVALUE");
        hr = DeleteJsonPath(wzFile, elementPath);
    }
    else if (flags.test(FLAG_REPLACEJSONVALUE)) {
        WcaLog(LOGMSG_STANDARD, "FLAG_REPLACEJSONVALUE");
        hr = SetJsonPathObject(wzFile, elementPath, wzValue);
    }

    return hr;
}
