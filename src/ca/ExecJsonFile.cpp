#include "stdafx.h"
#include "JsonFile.h"

/******************************************************************
 * ExecJsonFile - entry point for JsonFile Custom Action
 *****************************************************************/
extern "C" UINT WINAPI ExecJsonFile(
    __in MSIHANDLE hInstall)
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
