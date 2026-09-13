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
 SchedJsonFileCore - shared body of the two scheduling custom actions.

 Walks the WixJsonFile table and queues, for the deferred ExecJsonFile
 action, every write row whose On timing and component state match the
 phase this scheduler runs in (see JsonRowRunsInPhase), capturing each
 target file for ExecJsonFileRollback first.
********************************************************************/
static UINT SchedJsonFileCore(
    __in MSIHANDLE hInstall,
    __in_z LPCSTR szLogName,
    __in eJsonPhase phase
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
    hr = WcaInitialize(hInstall, szLogName);
    ExitOnFailure(hr, "failed to initialize")

    hr = ReadJsonFileTable(&pxfcHead, &pxfcTail);
    if (S_FALSE == hr)
    {
        WcaLog(LOGMSG_VERBOSE, "Skipping %s because WixJsonFile table not present", szLogName);
        ExitFunction1(hr = S_OK)
    }

    MessageExitOnFailure(hr, msierrJsonFileFailedRead, "failed to read WixJsonFile table")

    WcaLog(LOGMSG_VERBOSE, "Finished reading WixJsonFile table (%s phase)", jpUninstall == phase ? "uninstall" : "install");
    // loop through all the json configurations
    for (pxfc = pxfcHead; pxfc; pxfc = pxfc->pxfcNext)
    {
        // Only rows timed for this phase whose component is making the matching transition.
        if (JsonRowRunsInPhase(pxfc->iOn, pxfc->isInstalled, pxfc->isAction, phase))
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
            WcaLog(LOGMSG_VERBOSE, "WixJsonFile: Scheduling %s-time operation (flags=%d, on=%d) for file: %ls",
                jpUninstall == phase ? "uninstall" : "install", pxfc->iJsonFlags, pxfc->iOn, pxfc->wzFile);

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

/******************************************************************
 SchedJsonFile - install-time scheduler, sequenced after InstallFiles.
 Queues the rows with On=install/both of components being installed or
 repaired.
********************************************************************/
extern "C" UINT __stdcall SchedJsonFile(
    __in MSIHANDLE hInstall
)
{
    return SchedJsonFileCore(hInstall, "SchedJsonFile", jpInstall);
}

/******************************************************************
 SchedJsonFileUninstall - uninstall-time scheduler, sequenced before
 RemoveFiles so the target files still exist when the deferred action
 runs. Queues the rows with On=uninstall/both of components being
 uninstalled.
********************************************************************/
extern "C" UINT __stdcall SchedJsonFileUninstall(
    __in MSIHANDLE hInstall
)
{
    return SchedJsonFileCore(hInstall, "SchedJsonFileUninstall", jpUninstall);
}
