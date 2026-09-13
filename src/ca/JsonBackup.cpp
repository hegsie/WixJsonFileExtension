#include "stdafx.h"
#include "JsonFile.h"

// CreateBackup / RestoreOnUninstall support.
//
// A backup is <file><suffix>, taken once: before the first modification the extension makes to the
// file, and only if no backup exists yet. It therefore preserves the file as it was before the
// product first touched it; a repair, which re-applies the same modifications, does not overwrite
// it. The restore copies it back and removes it, so a later reinstall starts a fresh backup.

std::wstring JsonBackupPath(__in_z LPCWSTR wzFile, __in_z_opt LPCWSTR wzSuffix)
{
    std::wstring path = wzFile ? wzFile : L"";
    path += (wzSuffix && *wzSuffix) ? wzSuffix : L".wixbak";
    return path;
}

HRESULT BackupJsonFile(__in_z LPCWSTR wzFile, __in_z_opt LPCWSTR wzSuffix, __out_opt bool* pfCreated)
{
    if (pfCreated) { *pfCreated = false; }

    if (NULL == wzFile || L'\0' == *wzFile)
    {
        return E_INVALIDARG;
    }

    try
    {
        std::wstring backup = JsonBackupPath(wzFile, wzSuffix);

        if (!fs::exists(fs::path(wzFile)))
        {
            WcaLog(LOGMSG_STANDARD, "WixJsonFile: No backup of '%ls' taken - the file does not exist", wzFile);
            return S_FALSE;
        }

        if (fs::exists(fs::path(backup)))
        {
            WcaLog(LOGMSG_STANDARD, "WixJsonFile: Backup '%ls' already exists; keeping it (it holds the file as it was before the first modification)", backup.c_str());
            return S_FALSE;
        }

        if (!::CopyFileW(wzFile, backup.c_str(), TRUE))
        {
            DWORD dwError = ::GetLastError();
            WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - failed to back up '%ls' to '%ls' (error=%u)", wzFile, backup.c_str(), dwError);
            return HRESULT_FROM_WIN32(dwError ? dwError : ERROR_WRITE_FAULT);
        }

        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Backed up '%ls' to '%ls'", wzFile, backup.c_str());
        if (pfCreated) { *pfCreated = true; }
        return S_OK;
    }
    catch (const std::exception& e)
    {
        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - exception backing up '%ls': %s", wzFile, e.what());
        return E_FAIL;
    }
}

HRESULT RestoreJsonFileBackup(__in_z LPCWSTR wzFile, __in_z_opt LPCWSTR wzSuffix, __out_opt bool* pfRestored)
{
    if (pfRestored) { *pfRestored = false; }

    if (NULL == wzFile || L'\0' == *wzFile)
    {
        return E_INVALIDARG;
    }

    try
    {
        std::wstring backup = JsonBackupPath(wzFile, wzSuffix);

        if (!fs::exists(fs::path(backup)))
        {
            WcaLog(LOGMSG_STANDARD, "WixJsonFile: No backup '%ls' to restore over '%ls'", backup.c_str(), wzFile);
            return S_FALSE;
        }

        // Copy rather than move so a failure part-way leaves the backup in place.
        if (!::CopyFileW(backup.c_str(), wzFile, FALSE))
        {
            DWORD dwError = ::GetLastError();
            WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - failed to restore '%ls' from '%ls' (error=%u)", wzFile, backup.c_str(), dwError);
            return HRESULT_FROM_WIN32(dwError ? dwError : ERROR_WRITE_FAULT);
        }

        std::error_code ec;
        fs::remove(fs::path(backup), ec);
        if (ec)
        {
            WcaLog(LOGMSG_STANDARD, "WixJsonFile: Warning - restored '%ls' but could not remove backup '%ls': %s", wzFile, backup.c_str(), ec.message().c_str());
        }

        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Restored '%ls' from '%ls'", wzFile, backup.c_str());
        if (pfRestored) { *pfRestored = true; }
        return S_OK;
    }
    catch (const std::exception& e)
    {
        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - exception restoring '%ls': %s", wzFile, e.what());
        return E_FAIL;
    }
}
