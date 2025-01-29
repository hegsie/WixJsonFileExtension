#include "stdafx.h"
#include "JsonFile.h"

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
