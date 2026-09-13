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
    LPWSTR sczPhase = NULL;
    LPWSTR sczTransformLog = NULL;

    int iFlags = 0;
    int iIndex = -1;
    int iOptions = 0;
    bool fTransformLog = false;   // declared before the first ExitOnFailure: LExit is in its scope

    hr = WcaInitialize(hInstall, "ExecJsonFile");
    ExitOnFailure(hr, "WixJsonFile: Failed to initialize ExecJsonFile")

    hr = WcaGetProperty(L"CustomActionData", &pwzCustomActionData);
    WcaLog(LOGMSG_TRACEONLY, "WixJsonFile: CustomActionData: %ls", pwzCustomActionData);
    ExitOnFailure(hr, "WixJsonFile: Failed to get CustomActionData")

    pwz = pwzCustomActionData;

    // Header written by the scheduler: options, phase, transform log path.
    hr = WcaReadIntegerFromCaData(&pwz, &iOptions);
    ExitOnFailure(hr, "WixJsonFile: Failed to read options from custom action data")
    hr = WcaReadStringFromCaData(&pwz, &sczPhase);
    ExitOnFailure(hr, "WixJsonFile: Failed to read phase from custom action data")
    hr = WcaReadStringFromCaData(&pwz, &sczTransformLog);
    ExitOnFailure(hr, "WixJsonFile: Failed to read transform log path from custom action data")

    fTransformLog = sczTransformLog && *sczTransformLog;
    if (iOptions & JSON_OPTION_DRYRUN)
    {
        WcaLog(LOGMSG_STANDARD, "WixJsonFile: DRY RUN (%ls phase) - no JSON file will be modified", sczPhase);
    }

    // loop through all the passed in data
    while (pwz && *pwz)
    {
        hr = WcaReadIntegerFromCaData(&pwz, &iFlags);
        ExitOnFailure(hr, "WixJsonFile: Failed to get Flags for WixJsonFile")

        hr = WcaReadStringFromCaData(&pwz, &sczFile);
        ExitOnFailure(hr, "WixJsonFile: Failed to read file name from custom action data")

        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Configuring JSON file: %ls (flags=%d)", sczFile, iFlags);

        // Check if file exists before attempting operations
        if (sczFile && *sczFile)
        {
            DWORD dwAttrib = ::GetFileAttributesW(sczFile);
            if (dwAttrib == INVALID_FILE_ATTRIBUTES)
            {
                DWORD dwError = ::GetLastError();
                WcaLog(LOGMSG_STANDARD, "WixJsonFile: WARNING - File does not exist or is inaccessible: %ls (error=%d)", sczFile, dwError);
            }
            else
            {
                WcaLog(LOGMSG_VERBOSE, "WixJsonFile: File exists: %ls (attrib=0x%08X)", sczFile, dwAttrib);
            }
        }

        // Get path, name, and value to be written
        hr = WcaReadStringFromCaData(&pwz, &sczElementPath);
        ExitOnFailure(hr, "WixJsonFile: Failed to get ElementPath for file '%ls'", sczFile)

        hr = WcaReadStringFromCaData(&pwz, &sczValue);
        ExitOnFailure(hr, "WixJsonFile: Failed to process CustomActionData for file '%ls'", sczFile)

        hr = WcaReadIntegerFromCaData(&pwz, &iIndex);
        ExitOnFailure(hr, "WixJsonFile: Failed to get Index for WixJsonFile")

        hr = WcaReadStringFromCaData(&pwz, &sczSchemaFile);
        ExitOnFailure(hr, "WixJsonFile: Failed to get SchemaFile for WixJsonFile")

        JSON_OPERATION_TRACE trace;
        hr = UpdateJsonFile(sczFile, sczElementPath, sczValue, iFlags, iIndex, sczSchemaFile, iOptions, fTransformLog ? &trace : NULL);

        if (fTransformLog)
        {
            // One record per operation, written before a failure aborts the loop so the log
            // shows what failed. Snapshots are stored as JSON when they parse as such (the
            // normal case) and as the marker string otherwise ("<no match>", ...).
            try
            {
                std::string fileUtf8, pathUtf8, valueUtf8, schemaUtf8, phaseUtf8;
                WideToUtf8(sczFile, fileUtf8);
                WideToUtf8(sczElementPath, pathUtf8);
                WideToUtf8(sczValue ? sczValue : L"", valueUtf8);
                WideToUtf8(sczSchemaFile ? sczSchemaFile : L"", schemaUtf8);
                WideToUtf8(sczPhase ? sczPhase : L"", phaseUtf8);

                auto snapshot = [](const std::string& text) -> json
                {
                    try { return json::parse(text); }
                    catch (const std::exception&) { return json(text); }
                };

                SYSTEMTIME st;
                ::GetSystemTime(&st);
                char szTime[32];
                ::StringCchPrintfA(szTime, std::size(szTime), "%04u-%02u-%02uT%02u:%02u:%02uZ",
                    st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

                char szHr[16];
                ::StringCchPrintfA(szHr, std::size(szHr), "0x%08X", static_cast<unsigned int>(hr));

                json entry;
                entry["timestamp"] = std::string(szTime);
                entry["phase"] = phaseUtf8;
                entry["file"] = fileUtf8;
                entry["action"] = JsonActionName(iFlags);
                entry["elementPath"] = pathUtf8;
                entry["value"] = valueUtf8;
                entry["index"] = iIndex;
                entry["flags"] = iFlags;
                if (!schemaUtf8.empty()) { entry["schemaFile"] = schemaUtf8; }
                entry["outcome"] = trace.outcome;
                entry["hresult"] = std::string(szHr);
                entry["before"] = snapshot(trace.before);
                if (!trace.after.empty()) { entry["after"] = snapshot(trace.after); }

                HRESULT hrLog = AppendTransformLogEntry(sczTransformLog, entry);
                if (FAILED(hrLog))
                {
                    WcaLog(LOGMSG_STANDARD, "WixJsonFile: Warning - could not write transform log '%ls' (hr=0x%08X); continuing", sczTransformLog, static_cast<unsigned int>(hrLog));
                }
            }
            catch (const std::exception& e)
            {
                WcaLog(LOGMSG_STANDARD, "WixJsonFile: Warning - transform log entry failed: %s; continuing", e.what());
            }
        }

        ExitOnFailure(hr, "WixJsonFile: Failed while updating file '%ls' at path '%ls'", sczFile, sczElementPath)
    }

LExit:
    ReleaseStr(pwzCustomActionData)
    ReleaseStr(sczFile)
    ReleaseStr(sczElementPath)
    ReleaseStr(sczValue)
    ReleaseStr(sczSchemaFile)
    ReleaseStr(sczPhase)
    ReleaseStr(sczTransformLog)

    DWORD er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(er);
}
