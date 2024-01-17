
#include "stdafx.h"

#include <atlbase.h>
#include <atlconv.h>


using namespace jsoncons;
namespace fs = std::filesystem;

LPCWSTR vcsJsonFileQuery = L"SELECT `Wix4JsonFile`.`JsonConfig`, `Wix4JsonFile`.`File`, `Wix4JsonFile`.`ElementPath`, "
L"`Wix4JsonFile`.`Value`, `Wix4JsonFile`.`Flags`, `Wix4JsonFile`.`Component_` FROM `Wix4JsonFile`,`Component`"
L"WHERE `Wix4JsonFile`.`Component_`=`Component`.`Component` ORDER BY `File`, `Sequence`";
enum eJsonFileQuery { jfqId = 1, jfqFile, jfqElementPath, jfqValue, jfqFlags, jfqComponent };

static HRESULT UpdateJsonFile(
    __in_z LPCWSTR wzId,
    __in_z LPCWSTR wzFile,
    __in_z LPCWSTR wzElementPath,
    __in_z LPCWSTR wzValue,
    __in int iFlags,
    __in_z LPCWSTR wzComponent
);
HRESULT SetJsonPathValue(__in_z LPCWSTR wzFile, const std::string& sElementPath, __in_z LPCWSTR wzValue, bool createValue);
HRESULT SetJsonPathObject(__in_z LPCWSTR wzFile, std::string sElementPath, __in_z LPCWSTR wzValue);
HRESULT DeleteJsonPath(__in_z LPCWSTR wzFile, std::string sElementPath);

const int FLAG_DELETEVALUE = 0;
const int FLAG_SETVALUE = 1;
const int FLAG_ADDARRAYVALUE = 2;
const int FLAG_CREATEVALUE = 3;


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

extern "C" UINT WINAPI JsonFile(
    __in MSIHANDLE hInstall
)
{
    HRESULT hr = S_OK;
    hr = WcaInitialize(hInstall, "Wix4JsonFile");

    WcaLog(LOGMSG_STANDARD, "Entered Wix4JsonFile CA");
        
    WcaLog(LOGMSG_STANDARD, "Created PMSIHANDLE hView");
    PMSIHANDLE hView;

    WcaLog(LOGMSG_STANDARD, "Created PMSIHANDLE hRec");
    PMSIHANDLE hRec;

    WcaLog(LOGMSG_STANDARD, "MSIHANDLE's created for Wix4JsonFile CA");

    LPWSTR sczId = NULL;
    LPWSTR sczFile = NULL;
    LPWSTR sczElementPath = NULL;
    LPWSTR sczValue = NULL;
    LPWSTR sczComponent = NULL;

    INSTALLSTATE isInstalled;
    INSTALLSTATE isAction;

    int iFlags = 0;
        
    ExitOnFailure(hr, "Failed to initialize Wix4JsonFile.")

    // anything to do?
    if (S_OK != WcaTableExists(L"Wix4JsonFile"))
    {
        WcaLog(LOGMSG_STANDARD, "Wix4JsonFile table doesn't exist, so there are no .json files to update.");
        ExitFunction()
    }

    // query and loop through all the remove folders exceptions
    hr = WcaOpenExecuteView(vcsJsonFileQuery, &hView);
    ExitOnFailure(hr, "Failed to open view on Wix4JsonFile table")

    while (S_OK == (hr = WcaFetchRecord(hView, &hRec)))
    {
        hr = WcaGetRecordString(hRec, jfqId, &sczId);
        ExitOnFailure(hr, "Failed to get Wix4JsonFile identity.")

        hr = WcaGetRecordFormattedString(hRec, jfqFile, &sczFile);
        ExitOnFailure(hr, "failed to get File for Wix4JsonFile with Id: %s", sczId)

        hr = WcaGetRecordString(hRec, jfqElementPath, &sczElementPath);
        WcaLog(LOGMSG_STANDARD, "Found ElementPath :%ls", sczElementPath);
        ExitOnFailure(hr, "Failed to get ElementPath for Wix4JsonFile with Id: %s", sczId)

        WcaLog(LOGMSG_STANDARD, "Getting Value :%ls", sczId);
        hr = WcaGetRecordFormattedString(hRec, jfqValue, &sczValue);
        ExitOnFailure(hr, "Failed to get Value for Wix4JsonFile with Id: %s", sczId)

        hr = WcaGetRecordInteger(hRec, jfqFlags, &iFlags);
        ExitOnFailure(hr, "Failed to get Flags for Wix4JsonFile with Id: %s", sczId)

        hr = WcaGetRecordString(hRec, jfqComponent, &sczComponent);
        ExitOnFailure(hr, "Failed to get Wix4JsonFile component.")

        UINT er = ::MsiGetComponentStateW(hInstall, sczComponent, &isInstalled, &isAction);
        ExitOnFailure(hr = HRESULT_FROM_WIN32(er), "failed to get install state for Component: %ls", sczComponent)
        if (WcaIsInstalling(isInstalled, isAction))
        {
            hr = UpdateJsonFile(sczId, sczFile, sczElementPath, sczValue, iFlags, sczComponent);
            ExitOnFailure(hr, "Failed while navigating path: %S for row: %S", sczFile, sczId)
        }
        else if (WcaIsUninstalling(isInstalled, isAction)) {
            // Don't really worry about this yet as file is deleted on uninstall
        }
    }

    // reaching the end of the list is actually a good thing, not an error
    if (E_NOMOREITEMS == hr)
    {
        hr = S_OK;
    }
    ExitOnFailure(hr, "Failure occured while processing Wix4JsonFile table")

LExit:
    ReleaseStr(sczId)
    ReleaseStr(sczFile)
    ReleaseStr(sczElementPath)
    ReleaseStr(sczValue)
    ReleaseStr(sczComponent)

    if (hView)
    {
        ::MsiCloseHandle(hView);
        WcaLog(LOGMSG_STANDARD, "Closed PMSIHANDLE hView");
    }

    if (hRec)
    {
        ::MsiCloseHandle(hRec);
        WcaLog(LOGMSG_STANDARD, "Closed PMSIHANDLE hRec");
    }

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
    __in_z LPCWSTR wzId,
    __in_z LPCWSTR wzFile,
    __in_z LPCWSTR wzElementPath,
    __in_z LPCWSTR wzValue,
    __in int iFlags,
    __in_z LPCWSTR wzComponent
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

    WcaLog(LOGMSG_STANDARD, "Found ElementPath as %s", elementPath.c_str());
    
    elementPath = std::regex_replace(elementPath, std::regex(R"(\[(\\\[)\])"), "[");
    WcaLog(LOGMSG_STANDARD, "Updated ElementPath [ to %s", elementPath.c_str());

    //MessageBox(
    //    NULL,
    //    std::wstring(CA2W(std::string(elementPath).c_str())).c_str(),
    //    L"ELem Path Contains...",
    //    MB_OK
    //);

    elementPath = std::regex_replace(elementPath, std::regex(R"(\[(\\\])\])"), "]");
    WcaLog(LOGMSG_STANDARD, "Updated ElementPath ] to %s", elementPath.c_str());

    //MessageBox(
    //    NULL,
    //    std::wstring(CA2W(std::string(elementPath).c_str())).c_str(),
    //    L"ELem Path Contains...",
    //    MB_OK
    //);

    bool create = flags.test(FLAG_CREATEVALUE);
    WcaLog(LOGMSG_STANDARD, "Found create set to %s", create ? "true" : "false");
    if (flags.test(FLAG_SETVALUE) || create) {
        hr = SetJsonPathValue(wzFile, elementPath, wzValue, create);
    }
    else if (flags.test(FLAG_DELETEVALUE)) {
        hr = DeleteJsonPath(wzFile, elementPath);
    }
    else if (flags.test(FLAG_ADDARRAYVALUE)) {
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

        if (fs::exists(fs::path (wzFile))) {

            std::ifstream is(cFile);

            if (!is.is_open())
            {
                hr = ReturnLastError("Opening the file stream");
                if (FAILED(hr)) return hr;
            }

            json j = json::parse(is);

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

            auto query = jsonpath::json_query(j, sElementPath);
/*
            MessageBox(
                NULL,
                std::wstring(CA2W(std::string(sElementPath).c_str())).c_str(),
                L"DeleteJsonPath ELem Path Contains...",
                MB_OK
            );      */      

            WcaLog(LOGMSG_STANDARD, "About to delete value: $%s$ with value(s) %s ", sElementPath.c_str(),query.to_string().c_str());

            auto deleter = [](const json::string_view_type& path, json& val)
            {
                if (val.is_object())
                    val.erase(val.object_range().begin(), val.object_range().end());
                else
                    val.clear();
            };

            jsonpath::json_replace(j, sElementPath, deleter);

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
