#include "stdafx.h"
#include "JsonFile.h"

/******************************************************************
 * ExecJsonFileRollback - entry point for JsonFile rollback Custom Action
 *****************************************************************/
extern "C" UINT WINAPI ExecJsonFileRollback(
    __in MSIHANDLE hInstall
    )
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    LPWSTR pwzCustomActionData = NULL;
    LPWSTR pwz = NULL;
    LPWSTR pwzFileName = NULL;
    LPBYTE pbData = NULL;
    DWORD_PTR cbData = 0;

    FILETIME ft;

    HANDLE hFile = INVALID_HANDLE_VALUE;

    // initialize
    hr = WcaInitialize(hInstall, "ExecJsonFileRollback");
    ExitOnFailure(hr, "failed to initialize");

    hr = WcaGetProperty(L"CustomActionData", &pwzCustomActionData);
    ExitOnFailure(hr, "failed to get CustomActionData");

    WcaLog(LOGMSG_TRACEONLY, "CustomActionData: %ls", pwzCustomActionData);

    pwz = pwzCustomActionData;

    hr = WcaReadStringFromCaData(&pwz, &pwzFileName);
    ExitOnFailure(hr, "failed to read file name from custom action data");

    hr = WcaReadStreamFromCaData(&pwz, &pbData, &cbData);
    ExitOnFailure(hr, "failed to read file contents from custom action data");

    WcaLog(LOGMSG_VERBOSE, "Rolling back JSON file: %ls", pwzFileName);

    // Preserve the modified date on rollback
    hr = FileGetTime(pwzFileName, NULL, NULL, &ft);
    ExitOnFailure(hr, "Failed to get modified date of file %ls", pwzFileName);

    // Open the file
    hFile = ::CreateFileW(pwzFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    ExitOnInvalidHandleWithLastError(hFile, hr, "failed to open file for rollback: %ls", pwzFileName);

    // Write out the old data
    hr = FileWriteHandle(hFile, pbData, cbData);
    ExitOnFailure(hr, "failed to write to file during rollback: %ls", pwzFileName);

    ReleaseFile(hFile);

    // Preserve the modified date on rollback
    hr = FileSetTime(pwzFileName, NULL, NULL, &ft);
    ExitOnFailure(hr, "Failed to set modified date of file %ls", pwzFileName);

LExit:
    ReleaseStr(pwzCustomActionData);
    ReleaseStr(pwzFileName);
    ReleaseFile(hFile);
    ReleaseMem(pbData);

    return WcaFinalize(FAILED(hr) ? ERROR_INSTALL_FAILURE : er);
}
