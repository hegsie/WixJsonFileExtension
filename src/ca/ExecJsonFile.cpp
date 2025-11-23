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
    LPWSTR sczSchemaFile = NULL;

    int iFlags = 0;
    int iIndex = -1;

    hr = WcaInitialize(hInstall, "ExecJsonFile");
    ExitOnFailure(hr, "Failed to initialize ExecJsonFile")

    hr = WcaGetProperty(L"CustomActionData", &pwzCustomActionData);
    WcaLog(LOGMSG_TRACEONLY, "CustomActionData: %ls", pwzCustomActionData);
    ExitOnFailure(hr, "failed to get CustomActionData")

    pwz = pwzCustomActionData;

    // loop through all the passed in data
    while (pwz && *pwz)
    {
        hr = WcaReadIntegerFromCaData(&pwz, &iFlags);
        ExitOnFailure(hr, "Failed to get Flags for WixJsonFile")

        hr = WcaReadStringFromCaData(&pwz, &sczFile);
        ExitOnFailure(hr, "failed to read file name from custom action data")

        WcaLog(LOGMSG_VERBOSE, "Configuring JSON file: %ls", sczFile);

        // Get path, name, and value to be written
        hr = WcaReadStringFromCaData(&pwz, &sczElementPath);
        ExitOnFailure(hr, "Failed to get ElementPath for WixJsonFile")

        hr = WcaReadStringFromCaData(&pwz, &sczValue);
        ExitOnFailure(hr, "failed to process CustomActionData")

        hr = WcaReadIntegerFromCaData(&pwz, &iIndex);
        ExitOnFailure(hr, "Failed to get Index for WixJsonFile")

        hr = WcaReadStringFromCaData(&pwz, &sczSchemaFile);
        ExitOnFailure(hr, "Failed to get SchemaFile for WixJsonFile")

        hr = UpdateJsonFile(sczFile, sczElementPath, sczValue, iFlags, iIndex, sczSchemaFile);
        ExitOnFailure(hr, "Failed while updating file: %ls", sczFile)
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
    ReleaseStr(sczSchemaFile)

    DWORD er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}
