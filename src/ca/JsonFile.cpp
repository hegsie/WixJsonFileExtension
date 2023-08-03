
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
void SetJsonPathValue(__in_z LPCWSTR wzFile, std::string sElementPath, __in_z LPCWSTR wzValue);
void SetJsonPathObject(__in_z LPCWSTR wzFile, std::string sElementPath, __in_z LPCWSTR wzValue);
void DeleteJsonPath(__in_z LPCWSTR wzFile, std::string sElementPath);

const int FLAG_DELETEVALUE = 0;
const int FLAG_SETVALUE = 1;
const int FLAG_ADDARRAYVALUE = 2;
const int FLAG_UNINSTALL = 3;
const int FLAG_PRESERVEDATE = 4;
const int FLAG_JSONPOINTER = 5;

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
        
    ExitOnFailure(hr, "Failed to initialize Wix4JsonFile.");

    // anything to do?
    if (S_OK != WcaTableExists(L"Wix4JsonFile"))
    {
        WcaLog(LOGMSG_STANDARD, "Wix4JsonFile table doesn't exist, so there are no .json files to update.");
        ExitFunction();
    }

    // query and loop through all the remove folders exceptions
    hr = WcaOpenExecuteView(vcsJsonFileQuery, &hView);
    ExitOnFailure(hr, "Failed to open view on Wix4JsonFile table");

    while (S_OK == (hr = WcaFetchRecord(hView, &hRec)))
    {
        WcaLog(LOGMSG_STANDARD, "Getting Wix4JsonFile Id.");
        hr = WcaGetRecordString(hRec, jfqId, &sczId);
        ExitOnFailure(hr, "Failed to get Wix4JsonFile identity.");

        WcaLog(LOGMSG_STANDARD, "Getting Wix4JsonFile File for Id:%ls", sczId);
        hr = WcaGetRecordFormattedString(hRec, jfqFile, &sczFile);
        ExitOnFailure1(hr, "failed to get File for Wix4JsonFile with Id: %s", sczId);

        WcaLog(LOGMSG_STANDARD, "Getting Wix4JsonFile ElementPath for Id:%ls", sczId);
        hr = WcaGetRecordString(hRec, jfqElementPath, &sczElementPath);
        ExitOnFailure1(hr, "Failed to get ElementPath for Wix4JsonFile with Id: %s", sczId);

        WcaLog(LOGMSG_STANDARD, "Getting Wix4JsonFile Value for Id:%ls", sczId);
        hr = WcaGetRecordFormattedString(hRec, jfqValue, &sczValue);
        ExitOnFailure1(hr, "Failed to get Value for Wix4JsonFile with Id: %s", sczId);

        WcaLog(LOGMSG_STANDARD, "Getting Wix4JsonFile Flags for Id:%ls", sczId);
        hr = WcaGetRecordInteger(hRec, jfqFlags, &iFlags);
        ExitOnFailure1(hr, "Failed to get Flags for Wix4JsonFile with Id: %s", sczId);

        WcaLog(LOGMSG_STANDARD, "Getting Wix4JsonFile Component for Id:%ls", sczId);
        hr = WcaGetRecordString(hRec, jfqComponent, &sczComponent);
        ExitOnFailure(hr, "Failed to get Wix4JsonFile component.");

        UINT er = ::MsiGetComponentStateW(hInstall, sczComponent, &isInstalled, &isAction);
        ExitOnFailure1(hr = HRESULT_FROM_WIN32(er), "failed to get install state for Component: %ls", sczComponent);
        if (WcaIsInstalling(isInstalled, isAction))
        {
            WcaLog(LOGMSG_STANDARD, "Updating Wix4JsonFile for Id:%ls", sczId);
            hr = UpdateJsonFile(sczId, sczFile, sczElementPath, sczValue, iFlags, sczComponent);
            ExitOnFailure2(hr, "Failed while navigating path: %S for row: %S", sczFile, sczId);
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
    ExitOnFailure(hr, "Failure occured while processing Wix4JsonFile table");

LExit:
    ReleaseStr(sczId);
    ReleaseStr(sczFile);
    ReleaseStr(sczElementPath);
    ReleaseStr(sczValue);
    ReleaseStr(sczComponent);

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

    if (flags.test(FLAG_SETVALUE)) {
        SetJsonPathValue(wzFile, elementPath, wzValue);
    }
    else if (flags.test(FLAG_DELETEVALUE)) {
        DeleteJsonPath(wzFile, elementPath);
    }
    else if (flags.test(FLAG_ADDARRAYVALUE)) {
        SetJsonPathObject(wzFile, elementPath, wzValue);
    }

    return hr;
}

void SetJsonPathValue(__in_z LPCWSTR wzFile, std::string sElementPath, __in_z LPCWSTR wzValue) {

    try
    {
        _bstr_t bFile(wzFile);
        char* cFile = bFile;

        _bstr_t bValue(wzValue);
        char* cValue = bValue;

        if (fs::exists(fs::path (wzFile))) {
            
            std::ifstream is(cFile);
            json j = json::parse(is);

            WcaLog(LOGMSG_STANDARD, "About to replace value: |%s| {%s}", sElementPath.c_str(), cValue);

            auto f = [cValue](const std::string& /*path*/, json& value)
            {
                value = cValue;
            };

            jsonpath::json_replace(j, sElementPath.c_str(), f);

            WcaLog(LOGMSG_STANDARD, "Updating the json %s with values %s.", sElementPath.c_str(), cValue);

            std::ofstream os(wzFile,
                std::ios_base::out | std::ios_base::trunc);
            WcaLog(LOGMSG_STANDARD, "created output stream");

            pretty_print(j).dump(os);
            WcaLog(LOGMSG_STANDARD, "dumped output stream");

            os.close();
            WcaLog(LOGMSG_STANDARD, "closed output stream");
        }
        else {
            WcaLog(LOGMSG_STANDARD, "Unable to locate file: %s", cFile);
        }
    }
    catch (std::exception& e)
    {
        WcaLog(LOGMSG_STANDARD, "encountered error %s", e.what());
        throw;
    }
}

void DeleteJsonPath(__in_z LPCWSTR wzFile, std::string sElementPath)
{
    try
    {
        _bstr_t bFile(wzFile);
        char* cFile = bFile;

        if (fs::exists(fs::path(wzFile))) {
            json j;
            std::ifstream is(cFile);

            is >> j;

            auto query = jsonpath::json_query(j, sElementPath.c_str());
/*
            MessageBox(
                NULL,
                std::wstring(CA2W(std::string(sElementPath).c_str())).c_str(),
                L"DeleteJsonPath ELem Path Contains...",
                MB_OK
            );      */      

            WcaLog(LOGMSG_STANDARD, "About to delete value: |%s| with value(s) %s ", sElementPath.c_str(),query.to_string().c_str());

            auto deleter = [](const json::string_view_type& path, json& val)
            {
                if (val.is_object())
                    val.erase(val.object_range().begin(), val.object_range().end());
                else
                    val.clear();
            };

            jsonpath::json_replace(j, sElementPath.c_str(), deleter);

            std::cout << j << std::endl;

            WcaLog(LOGMSG_STANDARD, "Deleted the json %s", sElementPath.c_str());

            std::ofstream os(wzFile,
                std::ios_base::out | std::ios_base::trunc);
            WcaLog(LOGMSG_STANDARD, "created output stream");

            pretty_print(j).dump(os);
            WcaLog(LOGMSG_STANDARD, "dumped output stream");

            os.close();
            WcaLog(LOGMSG_STANDARD, "closed output stream");
        }
        else {
            WcaLog(LOGMSG_STANDARD, "Unable to locate file: %s", cFile);
        }
    }
    catch (std::exception& e)
    {
        WcaLog(LOGMSG_STANDARD, "encountered error %s", e.what());
        throw;
    }
}

void SetJsonPathObject(__in_z LPCWSTR wzFile, std::string sElementPath, __in_z LPCWSTR wzValue) {

    try
    {
        _bstr_t bFile(wzFile);
        char* cFile = bFile;

        _bstr_t bValue(wzValue);
        char* cValue = bValue;

        if (fs::exists(fs::path(wzFile))) {
            json j;
            std::ifstream is(cFile);

            is >> j;
            WcaLog(LOGMSG_STANDARD, "About to replace value: |%s| {%s}", sElementPath.c_str(), cValue);

            std::string s = cValue;
            json obj = json::parse(s);

            WcaLog(LOGMSG_STANDARD, "Parsed the new value: |%s|", obj.to_string().c_str());

            auto query = jsonpath::json_query(j, sElementPath.c_str());

            WcaLog(LOGMSG_STANDARD, "About to update the json %s with values %s.", sElementPath.c_str(), query.as_string().c_str());
            
            auto f = [obj](const std::string& /*path*/, json& value)
            {
                value = obj;
            };

            jsonpath::json_replace(j, sElementPath.c_str(), f);

            WcaLog(LOGMSG_STANDARD, "Updated the json %s with values %s.", sElementPath.c_str(), obj.as_string().c_str());

            std::ofstream os(wzFile,
                std::ios_base::out | std::ios_base::trunc);
            WcaLog(LOGMSG_STANDARD, "created output stream");

            pretty_print(j).dump(os);
            WcaLog(LOGMSG_STANDARD, "dumped output stream");

            os.close();
            WcaLog(LOGMSG_STANDARD, "closed output stream");
        }
        else {
            WcaLog(LOGMSG_STANDARD, "Unable to locate file: %s", cFile);
        }
    }
    catch (std::exception& e)
    {
        WcaLog(LOGMSG_STANDARD, "encountered error %s", e.what());
        throw;
    }
}
