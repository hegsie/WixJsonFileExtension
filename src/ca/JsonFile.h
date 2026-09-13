#pragma once
#include "stdafx.h"

#include <vector>

using namespace jsoncons;
namespace fs = std::filesystem;

// Custom action decoration for multi-architecture support (following WiX Toolset pattern)
#if defined(_M_AMD64)
#define JSON_CUSTOM_ACTION_DECORATION(f) L"Wix" f L"_X64"
#elif defined(_M_IX86)
#define JSON_CUSTOM_ACTION_DECORATION(f) L"Wix" f L"_X86"
#elif defined(_M_ARM64)
#define JSON_CUSTOM_ACTION_DECORATION(f) L"Wix" f L"_A64"
#else
#define JSON_CUSTOM_ACTION_DECORATION(f) f
#endif

// Cost for progress bar calculations
#define COST_JSONFILE 1000

enum eJsonFileQuery { jfqId = 1, jfqFile, jfqElementPath, jfqValue, jfqDefaultValue, jfqFlags, jfqComponent, jfqProperty, jfqCompAttributes, jfqIndex, jfqSchemaFile, jfqOn, jfqBackupSuffix };

// Values of the WixJsonFile.On column (bits, so both = install | uninstall). A null column is
// treated as install, matching the compiler's default.
const int TIMING_INSTALL = 1;
const int TIMING_UNINSTALL = 2;

// The phase a scheduling custom action runs in.
enum eJsonPhase
{
    jpInstall,    // WixSchedJsonFile: after InstallFiles, rows of components being installed/repaired
    jpUninstall   // WixSchedJsonFileUninstall: before RemoveFiles, rows of components being uninstalled
};

// Decides whether a row runs in the given phase: its On column must include the phase and the
// component's state transition must match it. Pure (no MSI calls) so the unit tests cover it.
inline bool JsonRowRunsInPhase(int iOn, INSTALLSTATE isInstalled, INSTALLSTATE isAction, eJsonPhase phase)
{
    if (MSI_NULL_INTEGER == iOn)
    {
        iOn = TIMING_INSTALL;
    }

    if (jpUninstall == phase)
    {
        // WcaIsUninstalling: installed (local/source) and going absent/removed.
        bool fUninstalling = (INSTALLSTATE_ABSENT == isAction || INSTALLSTATE_REMOVED == isAction) &&
                             (INSTALLSTATE_LOCAL == isInstalled || INSTALLSTATE_SOURCE == isInstalled);
        return (iOn & TIMING_UNINSTALL) && fUninstalling;
    }

    // WcaIsInstalling: going local/source, or already local/source with no change requested (repair).
    bool fInstalling = INSTALLSTATE_LOCAL == isAction || INSTALLSTATE_SOURCE == isAction ||
                       (INSTALLSTATE_DEFAULT == isAction && (INSTALLSTATE_LOCAL == isInstalled || INSTALLSTATE_SOURCE == isInstalled));
    return (iOn & TIMING_INSTALL) && fInstalling;
}


// These are bit positions
const int FLAG_DELETEVALUE = 0;
const int FLAG_SETVALUE = 1;
const int FLAG_REPLACEJSONVALUE = 2;
const int FLAG_CREATEVALUE = 3;
const int FLAG_READVALUE = 4;
const int FLAG_APPENDARRAY = 5;
const int FLAG_INSERTARRAY = 6;
const int FLAG_REMOVEARRAYELEMENT = 7;
const int FLAG_VALIDATESCHEMA = 8;
const int FLAG_DISTINCTVALUES = 9;
const int FLAG_ONLYIFEXISTS = 10;
const int FLAG_CREATEBACKUP = 11;       // modifier: back the file up before its first modification
const int FLAG_RESTOREBACKUP = 12;      // modifier on authored rows: restore that backup on uninstall.
                                        // Alone, it marks a synthesized restore record in the deferred data.

// These are bits
enum eXmlAction
{
    jaDeleteValue = 1,
    jaSetValue = 2,
    jaReplaceJsonValue = 4,
    jaCreateJsonPointerValue = 8,
    jaReadValue = 16,
    jaAppendArray = 32,
    jaInsertArray = 64,
    jaRemoveArrayElement = 128,
    jaDistinctValues = 512
    // Note: ValidateSchema (256) and OnlyIfExists (1024) are flags, not actions
};

#define msierrJsonFileFailedRead         25530

template<typename T>
std::string ToString(const T& v)
{
    std::ostringstream ss;
    ss << v;
    return ss.str();
}

template<typename T>
T FromString(const std::string& str)
{
    std::istringstream ss(str);
    T ret;
    ss >> ret;
    return ret;
}

#define MAX_DARWIN_KEY 73
#define MAX_DARWIN_COLUMN 255

struct JSON_FILE_CHANGE
{
    WCHAR wzId[MAX_DARWIN_KEY];

    INSTALLSTATE isInstalled;
    INSTALLSTATE isAction;

    WCHAR wzFile[MAX_PATH];
    LPWSTR pwzElementPath;
    LPWSTR pwzValue;
    LPWSTR pwzDefaultValue;

    int iJsonFlags;
    int iCompAttributes;

    LPWSTR pwzProperty;
    int iIndex;
    LPWSTR pwzSchemaFile;
    int iOn;
    LPWSTR pwzBackupSuffix;

    JSON_FILE_CHANGE* pxfcPrev;
    JSON_FILE_CHANGE* pxfcNext;
};

HRESULT ReadJsonFileTable(
    __inout JSON_FILE_CHANGE** ppxfcHead,
    __inout JSON_FILE_CHANGE** ppxfcTail
);
void FreeJsonFileChangeList(
    __in JSON_FILE_CHANGE* pxfcHead
);
// Execution options, set from the JSONEXT_LOGLEVEL / JSONEXT_DRYRUN properties by the immediate
// scheduler and carried to the deferred action at the head of its CustomActionData.
const int JSON_OPTION_VERBOSE = 1;   // log the matched value before and after every operation
const int JSON_OPTION_DRYRUN = 2;    // log what would be done, write nothing

// What one operation did, for the verbose log and the transform log (JSONEXT_TRANSFORMLOG).
struct JSON_OPERATION_TRACE
{
    std::string before;   // serialized value(s) at ElementPath before the operation (see DescribeJsonAtPath)
    std::string after;    // ... and after it; empty when nothing was written
    std::string outcome;  // "applied", "skipped" (OnlyIfExists), "dry-run" or "failed"
};

HRESULT UpdateJsonFile(
    __in_z LPCWSTR wzFile,
    __in_z LPCWSTR wzElementPath,
    __in_z LPCWSTR wzValue,
    __in int iFlags,
    __in int iIndex,
    __in_z LPCWSTR wzSchemaFile,
    __in int iOptions = 0,
    __inout_opt JSON_OPERATION_TRACE* pTrace = NULL
);

// Name of the action selected by the flags ("setValue", ...), or "unknown".
const char* JsonActionName(__in int iFlags);

// Serializes the value(s) currently at elementPath in wzFile (JSON Pointer when fPointer, else
// JSONPath): the JSON text of the match (an array of matches for JSONPath), or a bracketed marker
// such as "<file not found>", "<no match>" or "<not valid JSON>". Truncated to a log-friendly size.
std::string DescribeJsonAtPath(__in_z LPCWSTR wzFile, const std::string& elementPath, bool fPointer);

// Appends one entry to the JSON transform log (a JSON array file), creating the file if needed.
HRESULT AppendTransformLogEntry(__in_z LPCWSTR wzLogFile, const json& entry);

// CreateBackup / RestoreOnUninstall (JsonBackup.cpp). A null or empty suffix means ".wixbak".
std::wstring JsonBackupPath(__in_z LPCWSTR wzFile, __in_z_opt LPCWSTR wzSuffix);
// Copies the file to its backup unless the backup already exists (S_FALSE) or the file is missing (S_FALSE).
HRESULT BackupJsonFile(__in_z LPCWSTR wzFile, __in_z_opt LPCWSTR wzSuffix, __out_opt bool* pfCreated);
// Copies the backup back over the file and removes the backup; S_FALSE when there is no backup.
HRESULT RestoreJsonFileBackup(__in_z LPCWSTR wzFile, __in_z_opt LPCWSTR wzSuffix, __out_opt bool* pfRestored);
HRESULT SetJsonPathValue(__in_z LPCWSTR wzFile, const std::string& sElementPath, __in_z LPCWSTR wzValue, bool createValue);
HRESULT SetJsonPathObject(__in_z LPCWSTR wzFile, const std::string& sElementPath, __in_z LPCWSTR wzValue);
HRESULT DeleteJsonPath(__in_z LPCWSTR wzFile, const std::string& sElementPath);
HRESULT AppendJsonArray(__in_z LPCWSTR wzFile, const std::string& sElementPath, __in_z LPCWSTR wzValue);
HRESULT InsertJsonArray(__in_z LPCWSTR wzFile, const std::string& sElementPath, __in_z LPCWSTR wzValue, int iIndex);
HRESULT RemoveJsonArrayElement(__in_z LPCWSTR wzFile, const std::string& sElementPath, __in_z LPCWSTR wzValue);
HRESULT DistinctJsonArray(__in_z LPCWSTR wzFile, const std::string& sElementPath);
HRESULT ValidateJsonSchema(__in_z LPCWSTR wzFile, __in_z LPCWSTR wzSchemaFile);

std::string GetLastErrorAsString();
HRESULT ReturnLastError(const std::string& action);

// Atomically serializes and writes a JSON document to a file (temp file + replace).
HRESULT WriteJsonOutput(__in_z LPCWSTR wzFile, const json& j);
// Converts an authored value to a typed JSON value; preserves string type when replacing a string.
json MakeJsonValue(const std::string& valueUtf8, const json* pExisting);

// Logs text verbatim at the standard level. WcaLog places the message in record field 0, which
// MSI formats: "[10]" there is a reference to (empty) record field 10 and vanishes from the log,
// and the "[\[]" escape does not survive either. Passing the text as field 1 of a "[1]" template
// inserts it untouched. Outside an MSI session (unit tests, jsoncli) there is nothing to log to.
inline void JsonLogRaw(const std::string& text)
{
    if (!WcaIsInitialized())
    {
        return;
    }
    PMSIHANDLE hRec = ::MsiCreateRecord(1);
    if (!hRec)
    {
        return;
    }
    std::string tmpl = std::string(WcaGetLogName()) + ":  [1]";
    ::MsiRecordSetStringA(hRec, 0, tmpl.c_str());
    ::MsiRecordSetStringA(hRec, 1, text.c_str());
    WcaProcessMessage(INSTALLMESSAGE_INFO, hRec);
}

inline HRESULT WideToUtf8(__in_z LPCWSTR wzInput, std::string& value)
{
    value.clear();

    if (NULL == wzInput)
    {
        return E_INVALIDARG;
    }

    DWORD dwFlags = 0;
#ifdef WC_ERR_INVALID_CHARS
    dwFlags = WC_ERR_INVALID_CHARS;
#endif

    int cchRequired = ::WideCharToMultiByte(CP_UTF8, dwFlags, wzInput, -1, NULL, 0, NULL, NULL);
    if (cchRequired <= 0)
    {
        return HRESULT_FROM_WIN32(::GetLastError());
    }

    std::vector<char> utf8Buffer(cchRequired);

    int cchWritten = ::WideCharToMultiByte(CP_UTF8, dwFlags, wzInput, -1, utf8Buffer.data(), cchRequired, NULL, NULL);
    if (cchWritten <= 0)
    {
        return HRESULT_FROM_WIN32(::GetLastError());
    }

    value.assign(utf8Buffer.data(), static_cast<size_t>(cchWritten - 1)); // exclude null terminator

    return S_OK;
}

inline HRESULT Utf8ToWide(__in_z LPCSTR szInput, std::wstring& value)
{
    value.clear();

    if (NULL == szInput)
    {
        return E_INVALIDARG;
    }

    DWORD dwFlags = 0;
#ifdef MB_ERR_INVALID_CHARS
    dwFlags = MB_ERR_INVALID_CHARS;
#endif

    int cchRequired = ::MultiByteToWideChar(CP_UTF8, dwFlags, szInput, -1, NULL, 0);
    if (cchRequired <= 0)
    {
        return HRESULT_FROM_WIN32(::GetLastError());
    }

    std::vector<wchar_t> wideBuffer(cchRequired);

    int cchWritten = ::MultiByteToWideChar(CP_UTF8, dwFlags, szInput, -1, wideBuffer.data(), cchRequired);
    if (cchWritten <= 0)
    {
        return HRESULT_FROM_WIN32(::GetLastError());
    }

    value.assign(wideBuffer.data(), static_cast<size_t>(cchWritten - 1)); // exclude null terminator

    return S_OK;
}
