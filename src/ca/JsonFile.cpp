
#include "stdafx.h"

#include <atlbase.h>

using namespace jsoncons;
namespace fs = std::filesystem;

LPCWSTR vcsJsonFileQuery = L"SELECT `WixJsonFile`.`JsonConfig`, `WixJsonFile`.`File`, `WixJsonFile`.`ElementPath`, "
L"`WixJsonFile`.`Value`, `WixJsonFile`.`DefaultValue`, `WixJsonFile`.`Flags`, `WixJsonFile`.`Component_`, `WixJsonFile`.`Property`, `Component`.`Attributes` FROM `WixJsonFile`,`Component`"
L"WHERE `WixJsonFile`.`Component_`=`Component`.`Component` ORDER BY `File`, `Sequence`";
enum eJsonFileQuery { jfqId = 1, jfqFile, jfqElementPath, jfqValue, jfqDefaultValue, jfqFlags, jfqComponent, jfqProperty, jfqCompAttributes };


static HRESULT UpdateJsonFile(
    __in_z LPCWSTR wzFile,
    __in_z LPCWSTR wzElementPath,
    __in_z LPCWSTR wzValue,
    __in int iFlags
);
HRESULT SetJsonPathValue(__in_z LPCWSTR wzFile, const std::string& sElementPath, __in_z LPCWSTR wzValue, bool createValue);
HRESULT SetJsonPathObject(__in_z LPCWSTR wzFile, std::string sElementPath, __in_z LPCWSTR wzValue);
HRESULT DeleteJsonPath(__in_z LPCWSTR wzFile, std::string sElementPath);

// These are bit positions
const int FLAG_DELETEVALUE = 0;
const int FLAG_SETVALUE = 1;
const int FLAG_REPLACEJSONVALUE = 2;
const int FLAG_CREATEVALUE = 3;
const int FLAG_READVALUE = 4;

// These are bits
enum eXmlAction
{
    jaDeleteValue = 1,
    jaSetValue = 2,
    jaReplaceJsonValue = 4,
    jaCreateJsonPointerValue = 8,
    jaReadValue = 16,
};

#define msierrJsonFileFailedRead         25530

template<typename T>
std::string ToString(const T& v)
{
    std::ostringstream ss;
    ss << v;
    return ss.str();
}

template<typename T>
T FromString(const std::string& str)
{
    std::istringstream ss(str);
    T ret;
    ss >> ret;
    return ret;
}


#define MAX_DARWIN_KEY 73
#define MAX_DARWIN_COLUMN 255

struct JSON_FILE_CHANGE
{
    WCHAR wzId[MAX_DARWIN_KEY];

    INSTALLSTATE isInstalled;
    INSTALLSTATE isAction;

    WCHAR wzFile[MAX_PATH];
    LPWSTR pwzElementPath;
    LPWSTR pwzValue;
    LPWSTR pwzDefaultValue;

    int iJsonFlags;
    int iCompAttributes;

    LPWSTR pwzProperty;

    JSON_FILE_CHANGE* pxfcPrev;
    JSON_FILE_CHANGE* pxfcNext;
};


static HRESULT AddJsonFileChangeToList(
    __inout JSON_FILE_CHANGE** ppxfcHead,
    __inout JSON_FILE_CHANGE** ppxfcTail
)
{
    Assert(ppxfcHead && ppxfcTail);

    HRESULT hr = S_OK;

    JSON_FILE_CHANGE* pxfc = static_cast<JSON_FILE_CHANGE*>(MemAlloc(sizeof(JSON_FILE_CHANGE), TRUE));
    ExitOnNull(pxfc, hr, E_OUTOFMEMORY, "failed to allocate memory for new xml file change list element")

        // Add it to the end of the list
        if (NULL == *ppxfcHead)
        {
            *ppxfcHead = pxfc;
            *ppxfcTail = pxfc;
        }
        else
        {
            Assert(*ppxfcTail && (*ppxfcTail)->pxfcNext == NULL);
            (*ppxfcTail)->pxfcNext = pxfc;
            pxfc->pxfcPrev = *ppxfcTail;
            *ppxfcTail = pxfc;
        }

LExit:
    return hr;
}


static HRESULT ReadJsonFileTable(
    __inout JSON_FILE_CHANGE** ppxfcHead,
    __inout JSON_FILE_CHANGE** ppxfcTail
)
{
    Assert(ppxfcHead && ppxfcTail);

    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    PMSIHANDLE hView = NULL;
    PMSIHANDLE hRec = NULL;

    LPWSTR pwzData = NULL;

    // check to see if necessary tables are specified
    if (S_FALSE == WcaTableExists(L"WixJsonFile"))
    {
        ExitFunction1(hr = S_FALSE)
    }

    // loop through all the xml configurations
    hr = WcaOpenExecuteView(vcsJsonFileQuery, &hView);
    ExitOnFailure(hr, "failed to open view on WixJsonFile table")

        while (S_OK == (hr = WcaFetchRecord(hView, &hRec)))
        {
            hr = AddJsonFileChangeToList(ppxfcHead, ppxfcTail);
            ExitOnFailure(hr, "failed to add xml file change to list")

            // Get record Id
            hr = WcaGetRecordString(hRec, jfqId, &pwzData);
            ExitOnFailure(hr, "failed to get WixJsonFile record Id")
            hr = StringCchCopyW((*ppxfcTail)->wzId, countof((*ppxfcTail)->wzId), pwzData);
            ExitOnFailure(hr, "failed to copy WixJsonFile record Id")

            // Get component name
            hr = WcaGetRecordString(hRec, jfqComponent, &pwzData);
            ExitOnFailure(hr, "failed to get component name for WixJsonFile: %ls", (*ppxfcTail)->wzId)

            // Get the component's state
            er = ::MsiGetComponentStateW(WcaGetInstallHandle(), pwzData, &(*ppxfcTail)->isInstalled, &(*ppxfcTail)->isAction);
            ExitOnFailure(hr = HRESULT_FROM_WIN32(er), "failed to get install state for Component: %ls", pwzData)

            // Get the json file
            hr = WcaGetRecordFormattedString(hRec, jfqFile, &pwzData);
            ExitOnFailure(hr, "failed to get xml file for WixJsonFile: %ls", (*ppxfcTail)->wzId)
            hr = StringCchCopyW((*ppxfcTail)->wzFile, countof((*ppxfcTail)->wzFile), pwzData);
            ExitOnFailure(hr, "failed to copy xml file path")

            // Get the table flags
            hr = WcaGetRecordInteger(hRec, jfqFlags, &(*ppxfcTail)->iJsonFlags);
            ExitOnFailure(hr, "failed to get WixJsonFile flags for WixJsonFile: %ls", (*ppxfcTail)->wzId)

            // Get the element path
            hr = WcaGetRecordFormattedString(hRec, jfqElementPath, &(*ppxfcTail)->pwzElementPath);
            ExitOnFailure(hr, "failed to get XPath for WixJsonFile: %ls", (*ppxfcTail)->wzId)

            // Get the value
            hr = WcaGetRecordFormattedString(hRec, jfqValue, &pwzData);
            ExitOnFailure(hr, "failed to get Value for WixJsonFile: %ls", (*ppxfcTail)->wzId)
            hr = StrAllocString(&(*ppxfcTail)->pwzValue, pwzData, 0);
            ExitOnFailure(hr, "failed to allocate buffer for value")

            // Get the default value
            hr = WcaGetRecordFormattedString(hRec, jfqDefaultValue, &pwzData);
            ExitOnFailure(hr, "failed to get Default Value for WixJsonFile: %ls", (*ppxfcTail)->wzId)
            hr = StrAllocString(&(*ppxfcTail)->pwzDefaultValue, pwzData, 0);
            ExitOnFailure(hr, "failed to allocate buffer for default value")

            // Get the component attributes
            hr = WcaGetRecordInteger(hRec, jfqCompAttributes, &(*ppxfcTail)->iCompAttributes);
            ExitOnFailure(hr, "failed to get component attributes for WixJsonFile: %ls", (*ppxfcTail)->wzId)

            // Get the default value
            hr = WcaGetRecordFormattedString(hRec, jfqProperty, &pwzData);
            ExitOnFailure(hr, "failed to get Property for WixJsonFile: %ls", (*ppxfcTail)->wzId)
            hr = StrAllocString(&(*ppxfcTail)->pwzProperty, pwzData, 0);
            ExitOnFailure(hr, "failed to allocate buffer for property")
        }

    // if we looped through all records all is well
    if (E_NOMOREITEMS == hr)
        hr = S_OK;
    ExitOnFailure(hr, "failed while looping through all objects to secure")

LExit:
    ReleaseStr(pwzData)

    return hr;
}

/******************************************************************
 SchedJsonFile - entry point for JsonFile Custom Action
********************************************************************/
extern "C" UINT __stdcall SchedJsonFile(
    __in MSIHANDLE hInstall
)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    LPWSTR pwzCurrentFile = NULL;

    PMSIHANDLE hView = NULL;
    PMSIHANDLE hRec = NULL;

    JSON_FILE_CHANGE* pxfcHead = NULL;
    JSON_FILE_CHANGE* pxfcTail = NULL;
    JSON_FILE_CHANGE* pxfc = NULL;

    LPWSTR pwzCustomActionData = NULL;

    DWORD cFiles = 0;

    // initialize
    hr = WcaInitialize(hInstall, "SchedJsonFile");
    ExitOnFailure(hr, "failed to initialize")

        hr = ReadJsonFileTable(&pxfcHead, &pxfcTail);
    if (S_FALSE == hr)
    {
        WcaLog(LOGMSG_VERBOSE, "Skipping SchedJsonFile because WixJsonFile table not present");
        ExitFunction1(hr = S_OK)
    }

    MessageExitOnFailure(hr, msierrJsonFileFailedRead, "failed to read WixJsonFile table")

        WcaLog(LOGMSG_STANDARD, "Finished Reading WixJsonFile");
    // loop through all the xml configurations
    for (pxfc = pxfcHead; pxfc; pxfc = pxfc->pxfcNext)
    {
        // If it's being installed
        if (WcaIsInstalling(pxfc->isInstalled, pxfc->isAction))
        {
            hr = WcaWriteStringToCaData(pxfc->wzFile, &pwzCustomActionData);
            WcaLog(LOGMSG_STANDARD, "pxfc->wzFile");
            ExitOnFailure(hr, "failed to write File to custom action data: %ls", pxfc->wzFile)

                std::bitset<32> flags(pxfc->iJsonFlags);
            // Install the change
            if (flags.test(FLAG_DELETEVALUE))
            {
                hr = WcaWriteIntegerToCaData((int)jaDeleteValue, &pwzCustomActionData);
                WcaLog(LOGMSG_STANDARD, "jaDeleteValue");
                ExitOnFailure(hr, "failed to write create element action indicator to custom action data")
            }
            else if (flags.test(FLAG_SETVALUE))
            {
                hr = WcaWriteIntegerToCaData((int)jaSetValue, &pwzCustomActionData);
                WcaLog(LOGMSG_STANDARD, "jaSetValue");
                ExitOnFailure(hr, "failed to write file indicator to custom action data")
            }
            else if (flags.test(FLAG_REPLACEJSONVALUE))
            {
                hr = WcaWriteIntegerToCaData((int)jaReplaceJsonValue, &pwzCustomActionData);
                WcaLog(LOGMSG_STANDARD, "jaReplaceJsonValue");
                ExitOnFailure(hr, "failed to write bulk write value action indicator to custom action data")
            }
            else if (flags.test(FLAG_CREATEVALUE))
            {
                hr = WcaWriteIntegerToCaData((int)jaCreateJsonPointerValue, &pwzCustomActionData);
                WcaLog(LOGMSG_STANDARD, "jaCreateJsonPointerValue");
                ExitOnFailure(hr, "failed to write delete value action indicator to custom action data")
            }
            else if (flags.test(FLAG_READVALUE))
            {
                continue;
            }

            hr = WcaWriteStringToCaData(pxfc->pwzElementPath, &pwzCustomActionData);
            WcaLog(LOGMSG_STANDARD, "pxfc->pwzElementPath");
            ExitOnFailure(hr, "failed to write ElementPath to custom action data: %ls", pxfc->pwzElementPath)

            hr = WcaWriteStringToCaData(pxfc->pwzValue, &pwzCustomActionData);
            WcaLog(LOGMSG_STANDARD, "pxfc->pwzValue");
            ExitOnFailure(hr, "failed to write Value to custom action data: %ls", pxfc->pwzValue)

            ++cFiles;
        }
    }

    WcaLog(LOGMSG_STANDARD, "Built File list!");

    // If we looped through all records all is well
    if (E_NOMOREITEMS == hr)
        hr = S_OK;
    ExitOnFailure(hr, "failed while looping through all objects to secure")

        // Schedule the custom action and add to progress bar
        if (pwzCustomActionData && *pwzCustomActionData)
        {
            Assert(0 < cFiles);

            WcaLog(LOGMSG_STANDARD, "About to WcaDoDeferredAction");
            hr = WcaDoDeferredAction(L"WixExecJsonFile_X64", pwzCustomActionData, cFiles * 1000);
            WcaLog(LOGMSG_STANDARD, "Finished WcaDoDeferredAction");
            ExitOnFailure(hr, "failed to schedule ExecJsonFile action")
        }

LExit:
    ReleaseStr(pwzCurrentFile)
        ReleaseStr(pwzCustomActionData)

        return WcaFinalize(FAILED(hr) ? ERROR_INSTALL_FAILURE : er);
}

/******************************************************************
 * ReadValueJsonFile - entry point for JsonFile Custom Action
 *****************************************************************/
extern "C" UINT WINAPI ReadValueJsonFile(
    __in MSIHANDLE hInstall
)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    PMSIHANDLE hView = NULL;
    PMSIHANDLE hRec = NULL;

    JSON_FILE_CHANGE* pxfcHead = NULL;
    JSON_FILE_CHANGE* pxfcTail = NULL;
    JSON_FILE_CHANGE* pxfc = NULL;

    LPWSTR pwzCustomActionData = NULL;

    DWORD cFiles = 0;

    // initialize
    hr = WcaInitialize(hInstall, "ReadValueJsonFile");
    ExitOnFailure(hr, "failed to initialize")

    hr = ReadJsonFileTable(&pxfcHead, &pxfcTail);
    if (S_FALSE == hr)
    {
        WcaLog(LOGMSG_VERBOSE, "Skipping ReadValueJsonFile because WixJsonFile table not present");
        ExitFunction1(hr = S_OK)
    }

    MessageExitOnFailure(hr, msierrJsonFileFailedRead, "failed to read WixJsonFile table")

    WcaLog(LOGMSG_STANDARD, "Finished Reading WixJsonFile");
    // loop through all the json configurations
    for (pxfc = pxfcHead; pxfc; pxfc = pxfc->pxfcNext)
    {
        // If it's being installed
        if (WcaIsInstalling(pxfc->isInstalled, pxfc->isAction))
        {
            std::bitset<32> flags(pxfc->iJsonFlags);
            if (flags.test(FLAG_DELETEVALUE) || 
                flags.test(FLAG_SETVALUE) || 
                flags.test(FLAG_REPLACEJSONVALUE) || 
                flags.test(FLAG_CREATEVALUE))
            {
                continue;
            }
            else if (flags.test(FLAG_READVALUE))
            {
                WcaSetProperty(pxfc->pwzProperty, pxfc->pwzDefaultValue);
                // Update property with value
            }

            ++cFiles;
        }
    }

    WcaLog(LOGMSG_STANDARD, "Built File list!");

    // If we looped through all records all is well
    if (E_NOMOREITEMS == hr)
        hr = S_OK;
    ExitOnFailure(hr, "failed while looping through all objects to secure")

LExit:
    return WcaFinalize(FAILED(hr) ? ERROR_INSTALL_FAILURE : er);
}

/******************************************************************
 * ExecJsonFile - entry point for JsonFile Custom Action
 *****************************************************************/
extern "C" UINT WINAPI ExecJsonFile(
    __in MSIHANDLE hInstall
)
{
    HRESULT hr = S_OK;

    LPWSTR pwzCustomActionData = NULL;
    LPWSTR pwz = NULL;

    LPWSTR sczId = NULL;
    LPWSTR sczFile = NULL;
    LPWSTR sczElementPath = NULL;
    LPWSTR sczValue = NULL;
    LPWSTR sczComponent = NULL;

    int iFlags = 0;

    WcaLog(LOGMSG_STANDARD, "Entered WixJsonFile CA");

    hr = WcaInitialize(hInstall, "ExecJsonFile");
    WcaLog(LOGMSG_STANDARD, "Initialized ExecJsonFile CA");
    ExitOnFailure(hr, "Failed to initialize ExecJsonFile.")

        hr = WcaGetProperty(L"CustomActionData", &pwzCustomActionData);
    WcaLog(LOGMSG_STANDARD, "CustomActionData: %ls", pwzCustomActionData);
    ExitOnFailure(hr, "failed to get CustomActionData")

        pwz = pwzCustomActionData;

    // loop through all the passed in data
    while (pwz && *pwz)
    {
        hr = WcaReadStringFromCaData(&pwz, &sczFile);
        ExitOnFailure(hr, "failed to read file name from custom action data")

            WcaLog(LOGMSG_STANDARD, "Configuring Json File: %ls", sczFile);

        hr = WcaReadIntegerFromCaData(&pwz, &iFlags);
        ExitOnFailure(hr, "Failed to get Flags for WixJsonFile")

            // Get path, name, and value to be written
            hr = WcaReadStringFromCaData(&pwz, &sczElementPath);
        ExitOnFailure(hr, "Failed to get ElementPath for WixJsonFile ")

            hr = WcaReadStringFromCaData(&pwz, &sczValue);
        ExitOnFailure(hr, "failed to process CustomActionData")

            hr = UpdateJsonFile(sczFile, sczElementPath, sczValue, iFlags);
        ExitOnFailure(hr, "Failed while updating file: %S ", sczFile)
    }

    // reaching the end of the list is actually a good thing, not an error
    if (E_NOMOREITEMS == hr)
    {
        hr = S_OK;
    }

LExit:
    ReleaseStr(sczFile)
        ReleaseStr(sczElementPath)
        ReleaseStr(sczValue)
        ReleaseStr(sczComponent)

        DWORD er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}


std::string GetLastErrorAsString()
{
    //Get the error message ID, if any.
    DWORD errorMessageID = ::GetLastError();
    if (errorMessageID == 0) {
        return std::string(); //No error message has been recorded
    }

    LPSTR messageBuffer = nullptr;

    //Ask Win32 to give us the string version of that message ID.
    //The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

    //Copy the error message into a std::string.
    std::string message(messageBuffer, size);

    //Free the Win32's string's buffer.
    LocalFree(messageBuffer);

    return message;
}

static HRESULT UpdateJsonFile(
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
    WcaLog(LOGMSG_STANDARD, "Created wifstream, %s", cFile);

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

    //MessageBox(
    //    NULL,
    //    wzElementPath,
    //    L"ELem Path Contains...",
    //    MB_OK
    //);

    //WcaLog(LOGMSG_STANDARD, "Found ElementPath as %s", std::string(elementPath).c_str());

    //elementPath = std::regex_replace(elementPath, std::regex(R"(\[(\\\[)\])"), "[");
    WcaLog(LOGMSG_STANDARD, "[Found ElementPath to %ls", wzElementPath);

    //MessageBox(
    //    NULL,
    //    std::wstring(CA2W(std::string(elementPath).c_str())).c_str(),
    //    L"ELem Path Contains...",
    //    MB_OK
    //);

    //elementPath = std::regex_replace(elementPath, std::regex(R"(\[(\\\])\])"), "]");
    //WcaLog(LOGMSG_STANDARD, "Updated ElementPath ] to %s", elementPath.c_str());

    //MessageBox(
    //    NULL,
    //    std::wstring(CA2W(std::string(elementPath).c_str())).c_str(),
    //    L"ELem Path Contains...",
    //    MB_OK
    //);

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

HRESULT ReturnLastError(std::string action)
{
    DWORD err = GetLastError();
    if (err != 0)
    {
        std::string errorString = GetLastErrorAsString();
        WcaLog(LOGMSG_STANDARD, "GetLastError returned %d, %s @ %s", err, errorString.c_str(), action.c_str());

        HRESULT hr = HRESULT_FROM_WIN32(err);

        WcaLog(LOGMSG_STANDARD, "HRESULT hr %d", hr);
        return hr;
    }
    return S_OK;
}

HRESULT SetJsonPathValue(__in_z LPCWSTR wzFile, const std::string& sElementPath, __in_z LPCWSTR wzValue, bool createValue) {

    try
    {
        _bstr_t bFile(wzFile);
        char* cFile = bFile;

        _bstr_t bValue(wzValue);
        char* cValue = bValue;
        HRESULT hr = S_OK;

        WcaLog(LOGMSG_STANDARD, "Checking if %ls Exists", wzFile);
        if (fs::exists(fs::path(wzFile))) {

            std::ifstream is(cFile);
            WcaLog(LOGMSG_STANDARD, "Opened %s", cFile);

            if (!is.is_open())
            {
                hr = ReturnLastError("Opening the file stream");
                if (FAILED(hr)) return hr;
            }

            json j = json::parse(is);
            WcaLog(LOGMSG_STANDARD, "Parsed File");

            if (createValue) {
                std::error_code ec;
                jsonpointer::add_if_absent(j, sElementPath, json(cValue), ec);

                if (ec) {
                    WcaLog(LOGMSG_STANDARD, "json pointer add_if_absent %s", ec.message());
                }
                else {
                    std::ofstream os(wzFile,
                        std::ios_base::out | std::ios_base::trunc);

                    pretty_print(j).dump(os);

                    os.close();
                }
            }
            else {

                json query = jsonpath::json_query(j, sElementPath);

                WcaLog(LOGMSG_STANDARD, "Found %d elements", query.size());

                if (query.size() > 0) {
                    auto f = [cValue](const std::string& /*path*/, json& value)
                        {
                            value = cValue;
                        };

                    jsonpath::json_replace(j, sElementPath, f);

                    hr = ReturnLastError("Replacing elements in the json");
                    if (FAILED(hr)) return hr;

                    WcaLog(LOGMSG_STANDARD, "Updating the json %s with values %s.", sElementPath.c_str(), cValue);

                    std::ofstream os(wzFile,
                        std::ios_base::out | std::ios_base::trunc);

                    if (!os.is_open())
                    {
                        hr = ReturnLastError("creating the output stream");
                        if (FAILED(hr)) return hr;
                    }

                    pretty_print(j).dump(os);
                    os.close();
                }
                else {
                    WcaLog(LOGMSG_STANDARD, "No elements to update: %s", sElementPath.c_str());

                    return HRESULT_FROM_WIN32(ERROR_OBJECT_NOT_FOUND);
                }
            }
        }
        else {
            WcaLog(LOGMSG_STANDARD, "Unable to locate file: %s", cFile);
        }
        return S_OK;
    }
    catch (std::exception& e)
    {
        WcaLog(LOGMSG_STANDARD, "encountered error %s", e.what());
        throw;
    }
}


HRESULT DeleteJsonPath(__in_z LPCWSTR wzFile, std::string sElementPath)
{
    try
    {
        _bstr_t bFile(wzFile);
        char* cFile = bFile;
        HRESULT hr = S_OK;

        if (fs::exists(fs::path(wzFile))) {
            json j;
            std::ifstream is(cFile);

            if (!is.is_open())
            {
                hr = ReturnLastError("Opening the file stream");
                if (FAILED(hr)) return hr;
            }

            is >> j;

            //MessageBox(
            //    NULL,
            //    std::wstring(CA2W(std::string(sElementPath).c_str())).c_str(),
            //    L"DeleteJsonPath ELem Path Contains...",
            //    MB_OK
            //);

            auto expr = jsonpath::make_expression<json>(sElementPath);
            std::vector<jsonpath::json_location> locations = expr.select_paths(j,
                jsonpath::result_options::sort_descending | jsonpath::result_options::sort_descending);

            //for (const jsonpath::json_location& location : locations)
            //{
            //    WcaLog(LOGMSG_STANDARD, "About to delete value: %s ", to_basic_string(location).c_str());
            //}

            for (const auto& location : locations)
            {
                jsonpath::remove(j, location);
            }

            std::cout << j << '\n';

            WcaLog(LOGMSG_STANDARD, "Deleted the json %s", sElementPath.c_str());

            std::ofstream os(wzFile,
                std::ios_base::out | std::ios_base::trunc);

            if (!os.is_open())
            {
                hr = ReturnLastError("creating the output stream");
                if (FAILED(hr)) return hr;
            }

            pretty_print(j).dump(os);
            os.close();
        }
        else {
            WcaLog(LOGMSG_STANDARD, "Unable to locate file: %s", cFile);
        }
        return S_OK;
    }
    catch (std::exception& e)
    {
        WcaLog(LOGMSG_STANDARD, "encountered error %s", e.what());
        throw;
    }
}

HRESULT SetJsonPathObject(__in_z LPCWSTR wzFile, std::string sElementPath, __in_z LPCWSTR wzValue) {

    try
    {
        _bstr_t bFile(wzFile);
        char* cFile = bFile;

        _bstr_t bValue(wzValue);
        char* cValue = bValue;
        HRESULT hr = S_OK;

        if (fs::exists(fs::path(wzFile))) {
            json j;
            std::ifstream is(cFile);

            is >> j;

            std::string s = cValue;
            json obj = json::parse(s);

            WcaLog(LOGMSG_STANDARD, "Parsed the new value: $%s$", obj.to_string().c_str());

            auto query = jsonpath::json_query(j, sElementPath);

            WcaLog(LOGMSG_STANDARD, "About to update the json %s with values %s.", sElementPath.c_str(), query.as_string().c_str());

            auto f = [obj](const std::string& /*path*/, json& value)
                {
                    value = obj;
                };

            jsonpath::json_replace(j, sElementPath, f);

            WcaLog(LOGMSG_STANDARD, "Updated the json %s with values %s.", sElementPath.c_str(), obj.as_string().c_str());

            std::ofstream os(wzFile,
                std::ios_base::out | std::ios_base::trunc);

            if (!os.is_open())
            {
                hr = ReturnLastError("creating the output stream");
                if (FAILED(hr)) return hr;
            }

            pretty_print(j).dump(os);

            os.close();
        }
        else {
            WcaLog(LOGMSG_STANDARD, "Unable to locate file: %s", cFile);
        }
        return S_OK;
    }
    catch (std::exception& e)
    {
        WcaLog(LOGMSG_STANDARD, "encountered error %s", e.what());
        throw;
    }
}
