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

    LPWSTR sczFile = NULL;
    LPWSTR sczElementPath = NULL;
    LPWSTR sczValue = NULL;

    int iFlags = 0;

    hr = WcaInitialize(hInstall, "ExecJsonFile");
    ExitOnFailure(hr, "WixJsonFile: Failed to initialize ExecJsonFile")

    hr = WcaGetProperty(L"CustomActionData", &pwzCustomActionData);
    WcaLog(LOGMSG_TRACEONLY, "WixJsonFile: CustomActionData: %ls", pwzCustomActionData);
    ExitOnFailure(hr, "WixJsonFile: Failed to get CustomActionData")

    pwz = pwzCustomActionData;

    // loop through all the passed in data
    while (pwz && *pwz)
    {
        hr = WcaReadIntegerFromCaData(&pwz, &iFlags);
        ExitOnFailure(hr, "WixJsonFile: Failed to get Flags for WixJsonFile")

        hr = WcaReadStringFromCaData(&pwz, &sczFile);
        ExitOnFailure(hr, "WixJsonFile: Failed to read file name from custom action data")

        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Configuring JSON file: %ls", sczFile);

        // Get path, name, and value to be written
        hr = WcaReadStringFromCaData(&pwz, &sczElementPath);
        ExitOnFailure(hr, "WixJsonFile: Failed to get ElementPath for file '%ls'", sczFile)

        hr = WcaReadStringFromCaData(&pwz, &sczValue);
        ExitOnFailure(hr, "WixJsonFile: Failed to process CustomActionData for file '%ls'", sczFile)

        hr = UpdateJsonFile(sczFile, sczElementPath, sczValue, iFlags);
        ExitOnFailure(hr, "WixJsonFile: Failed while updating file '%ls' at path '%ls'", sczFile, sczElementPath)
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

    DWORD er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}
