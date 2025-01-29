#include "stdafx.h"
#include "JsonFile.h"

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
