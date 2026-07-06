#include "stdafx.h"
#include "JsonFile.h"

// Helper function to schedule rollback for a file
static HRESULT ScheduleFileRollback(
    __in LPCWSTR wzFile,
    __inout LPWSTR* ppwzRollbackCustomActionData
    )
{
    HRESULT hr = S_OK;
    LPBYTE pbData = NULL;
    SIZE_T cbData = 0;

    // If the file already exists, save its state for rollback
    if (FileExistsEx(wzFile, NULL))
    {
        hr = FileRead(&pbData, &cbData, wzFile);
        ExitOnFailure(hr, "failed to read file for rollback: %ls", wzFile);

        hr = WcaWriteStringToCaData(wzFile, ppwzRollbackCustomActionData);
        ExitOnFailure(hr, "failed to write file name to rollback custom action data: %ls", wzFile);

        hr = WcaWriteStreamToCaData(pbData, cbData, ppwzRollbackCustomActionData);
        ExitOnFailure(hr, "failed to write file contents to rollback custom action data");

        WcaLog(LOGMSG_VERBOSE, "Scheduled rollback for file: %ls", wzFile);
    }

LExit:
    ReleaseMem(pbData);
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
    LPWSTR pwzRollbackCustomActionData = NULL;

    DWORD cFiles = 0;
    DWORD cUniqueFiles = 0;
    BOOL fScheduledRollback = FALSE;

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

    WcaLog(LOGMSG_VERBOSE, "Finished reading WixJsonFile table");
    // loop through all the json configurations
    for (pxfc = pxfcHead; pxfc; pxfc = pxfc->pxfcNext)
    {
        // If it's being installed
        if (WcaIsInstalling(pxfc->isInstalled, pxfc->isAction))
        {
            std::bitset<32> flags(pxfc->iJsonFlags);

            // readValue is handled by the immediate ReadValueJsonFile action, not the deferred one.
            if (flags.test(FLAG_READVALUE))
            {
                continue;
            }

            // Skip entries that don't carry a deferred write action.
            if (!(flags.test(FLAG_DELETEVALUE) || flags.test(FLAG_SETVALUE) || flags.test(FLAG_REPLACEJSONVALUE) ||
                  flags.test(FLAG_CREATEVALUE) || flags.test(FLAG_APPENDARRAY) || flags.test(FLAG_INSERTARRAY) ||
                  flags.test(FLAG_REMOVEARRAYELEMENT) || flags.test(FLAG_DISTINCTVALUES)))
            {
                WcaLog(LOGMSG_VERBOSE, "Unknown or no action flag set, skipping entry for file: %ls", pxfc->wzFile);
                continue;
            }

            // Pass the complete flag set (action plus modifiers such as OnlyIfExists and ValidateSchema)
            // so the deferred ExecJsonFile action sees exactly what was authored in the WixJsonFile table.
            hr = WcaWriteIntegerToCaData(pxfc->iJsonFlags, &pwzCustomActionData);
            ExitOnFailure(hr, "failed to write flags to custom action data")
            WcaLog(LOGMSG_VERBOSE, "WixJsonFile: Scheduling operation (flags=%d) for file: %ls", pxfc->iJsonFlags, pxfc->wzFile);

            // Schedule rollback for this file if we haven't already
            if (!pwzCurrentFile || 0 != lstrcmpW(pwzCurrentFile, pxfc->wzFile))
            {
                hr = StrAllocString(&pwzCurrentFile, pxfc->wzFile, 0);
                ExitOnFailure(hr, "failed to copy current file name");

                hr = ScheduleFileRollback(pwzCurrentFile, &pwzRollbackCustomActionData);
                ExitOnFailure(hr, "failed to schedule rollback for file: %ls", pwzCurrentFile);

                ++cUniqueFiles;
                fScheduledRollback = TRUE;
            }

            hr = WcaWriteStringToCaData(pxfc->wzFile, &pwzCustomActionData);
            ExitOnFailure(hr, "failed to write file to custom action data: %ls", pxfc->wzFile)

            hr = WcaWriteStringToCaData(pxfc->pwzElementPath, &pwzCustomActionData);
            WcaLog(LOGMSG_VERBOSE, "Element path: %ls", pxfc->pwzElementPath);
            ExitOnFailure(hr, "failed to write ElementPath to custom action data: %ls", pxfc->pwzElementPath)

            hr = WcaWriteStringToCaData(pxfc->pwzValue, &pwzCustomActionData);
            WcaLog(LOGMSG_VERBOSE, "Value: %ls", pxfc->pwzValue);
            ExitOnFailure(hr, "failed to write Value to custom action data: %ls", pxfc->pwzValue)

            hr = WcaWriteIntegerToCaData(pxfc->iIndex, &pwzCustomActionData);
            WcaLog(LOGMSG_VERBOSE, "Index: %d", pxfc->iIndex);
            ExitOnFailure(hr, "failed to write Index to custom action data")

            hr = WcaWriteStringToCaData(pxfc->pwzSchemaFile, &pwzCustomActionData);
            WcaLog(LOGMSG_VERBOSE, "Schema file: %ls", pxfc->pwzSchemaFile);
            ExitOnFailure(hr, "failed to write SchemaFile to custom action data: %ls", pxfc->pwzSchemaFile)

            ++cFiles;
        }
    }

    WcaLog(LOGMSG_VERBOSE, "Scheduled %d file operations", cFiles);

    // Schedule the rollback custom action first
    if (fScheduledRollback && pwzRollbackCustomActionData && *pwzRollbackCustomActionData)
    {
        Assert(0 < cUniqueFiles);
        WcaLog(LOGMSG_VERBOSE, "Scheduling rollback custom action for %d unique files", cUniqueFiles);
        hr = WcaDoDeferredAction(JSON_CUSTOM_ACTION_DECORATION(L"ExecJsonFileRollback"), pwzRollbackCustomActionData, cUniqueFiles * COST_JSONFILE);
        ExitOnFailure(hr, "failed to schedule ExecJsonFileRollback action")
    }

    // Schedule the deferred custom action and add to progress bar
    if (pwzCustomActionData && *pwzCustomActionData)
    {
        Assert(0 < cFiles);

        WcaLog(LOGMSG_VERBOSE, "Scheduling deferred custom action");
        hr = WcaDoDeferredAction(JSON_CUSTOM_ACTION_DECORATION(L"ExecJsonFile"), pwzCustomActionData, cFiles * COST_JSONFILE);
        ExitOnFailure(hr, "failed to schedule ExecJsonFile action")
    }

LExit:
    ReleaseStr(pwzCurrentFile)
    ReleaseStr(pwzCustomActionData)
    ReleaseStr(pwzRollbackCustomActionData)

    // Free the linked list to prevent memory leak
    if (pxfcHead)
    {
        FreeJsonFileChangeList(pxfcHead);
    }

    return WcaFinalize(FAILED(hr) ? ERROR_INSTALL_FAILURE : er);
}
