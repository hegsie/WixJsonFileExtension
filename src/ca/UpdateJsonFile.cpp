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
    ojson j = NULL;
    ::SetLastError(0);

    _bstr_t bFile(wzFile);
    char* cFile = bFile;

    _bstr_t bValue(wzValue);
    char* cValue = bValue;

    std::ifstream is(cFile);
    WcaLog(LOGMSG_STANDARD, "Created input file stream, %ls", wzFile);

    if (is.fail())
    {
        WcaLog(LOGMSG_STANDARD, "Unable to open the target file, either its missing or rights need adding/elevating");
        return 1;
    }

    json_stream_reader reader(is);

    std::error_code ec;
    reader.read(ec);

    if (ec)
    {
        WcaLog(LOGMSG_STANDARD, "%s on line %i and column %i", ec.message().c_str(), reader.line(), reader.column());
        std::cout << ec.message()
            << " on line " << reader.line()
            << " and column " << reader.column()
            << std::endl;
    }

    is.close();

    std::bitset<32> flags(iFlags);
    WcaLog(LOGMSG_STANDARD, "Using the following flags, %i, %s", iFlags, flags.test(FLAG_SETVALUE) ? "true" : "false");

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
