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

    LPWSTR pwzProperty = NULL;
    LPWSTR pwzTransformLog = NULL;
    int iOptions = 0;

    // initialize
    hr = WcaInitialize(hInstall, szLogName);
    ExitOnFailure(hr, "failed to initialize")

    // Diagnostic switches, public properties so they can be set on the msiexec command line:
    //   JSONEXT_LOGLEVEL=verbose   log the value at every ElementPath before and after each operation
    //   JSONEXT_DRYRUN=1           log every operation but write nothing (no rollback is scheduled either)
    //   JSONEXT_TRANSFORMLOG=path  append a JSON record of every operation to this file
    // They are read here, in the immediate phase, and passed to the deferred action in the
    // CustomActionData header because deferred actions cannot read properties.
    hr = WcaGetProperty(L"JSONEXT_LOGLEVEL", &pwzProperty);
    ExitOnFailure(hr, "failed to get JSONEXT_LOGLEVEL property")
    if (pwzProperty && 0 == _wcsicmp(pwzProperty, L"verbose"))
    {
        iOptions |= JSON_OPTION_VERBOSE;
    }

    // A verbose MSI log (/l*v, or MsiLogging with 'v') gets the snapshots too. Decided here
    // because the deferred action cannot read properties.
    hr = WcaGetProperty(L"MsiLogging", &pwzProperty);
    ExitOnFailure(hr, "failed to get MsiLogging property")
    if (pwzProperty && (wcschr(pwzProperty, L'v') || wcschr(pwzProperty, L'V')))
    {
        iOptions |= JSON_OPTION_VERBOSE;
    }

    hr = WcaGetProperty(L"JSONEXT_DRYRUN", &pwzProperty);
    ExitOnFailure(hr, "failed to get JSONEXT_DRYRUN property")
    if (pwzProperty && *pwzProperty &&
        (0 == _wcsicmp(pwzProperty, L"1") || 0 == _wcsicmp(pwzProperty, L"yes") || 0 == _wcsicmp(pwzProperty, L"true")))
    {
        iOptions |= JSON_OPTION_DRYRUN;
        WcaLog(LOGMSG_STANDARD, "WixJsonFile: JSONEXT_DRYRUN is set - JSON operations will be logged but not applied");
    }

    hr = WcaGetProperty(L"JSONEXT_TRANSFORMLOG", &pwzTransformLog);
    ExitOnFailure(hr, "failed to get JSONEXT_TRANSFORMLOG property")
    if (pwzTransformLog && *pwzTransformLog)
    {
        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Transform log: %ls", pwzTransformLog);
    }

    // CustomActionData header: options, phase, transform log path. The records follow.
    hr = WcaWriteIntegerToCaData(iOptions, &pwzCustomActionData);
    ExitOnFailure(hr, "failed to write options to custom action data")
    hr = WcaWriteStringToCaData(jpUninstall == phase ? L"uninstall" : L"install", &pwzCustomActionData);
    ExitOnFailure(hr, "failed to write phase to custom action data")
    hr = WcaWriteStringToCaData(pwzTransformLog ? pwzTransformLog : L"", &pwzCustomActionData);
    ExitOnFailure(hr, "failed to write transform log path to custom action data")

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

            // Schedule rollback for this file if we haven't already. A dry run writes nothing, so
            // there is nothing to roll back (and capturing every file would be wasted work).
            if (!(iOptions & JSON_OPTION_DRYRUN) && (!pwzCurrentFile || 0 != lstrcmpW(pwzCurrentFile, pxfc->wzFile)))
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

    // Schedule the deferred custom action and add to progress bar (the data always carries the
    // header, so check the record count rather than the string).
    if (0 < cFiles)
    {
        WcaLog(LOGMSG_VERBOSE, "Scheduling deferred custom action");
        hr = WcaDoDeferredAction(JSON_CUSTOM_ACTION_DECORATION(L"ExecJsonFile"), pwzCustomActionData, cFiles * COST_JSONFILE);
        ExitOnFailure(hr, "failed to schedule ExecJsonFile action")
    }

LExit:
    ReleaseStr(pwzProperty)
    ReleaseStr(pwzTransformLog)
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
