#pragma once
#include "stdafx.h"

#include <atlbase.h>

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

enum eJsonFileQuery { jfqId = 1, jfqFile, jfqElementPath, jfqValue, jfqDefaultValue, jfqFlags, jfqComponent, jfqProperty, jfqCompAttributes, jfqIndex, jfqSchemaFile };


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
    jaRemoveArrayElement = 128
    // Note: ValidateSchema (256) is a flag, not an action
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
HRESULT UpdateJsonFile(
    __in_z LPCWSTR wzFile,
    __in_z LPCWSTR wzElementPath,
    __in_z LPCWSTR wzValue,
    __in int iFlags,
    __in int iIndex,
    __in_z LPCWSTR wzSchemaFile
);
HRESULT SetJsonPathValue(__in_z LPCWSTR wzFile, const std::string& sElementPath, __in_z LPCWSTR wzValue, bool createValue);
HRESULT SetJsonPathObject(__in_z LPCWSTR wzFile, const std::string& sElementPath, __in_z LPCWSTR wzValue);
HRESULT DeleteJsonPath(__in_z LPCWSTR wzFile, const std::string& sElementPath);
HRESULT AppendJsonArray(__in_z LPCWSTR wzFile, const std::string& sElementPath, __in_z LPCWSTR wzValue);
HRESULT InsertJsonArray(__in_z LPCWSTR wzFile, const std::string& sElementPath, __in_z LPCWSTR wzValue, int iIndex);
HRESULT RemoveJsonArrayElement(__in_z LPCWSTR wzFile, const std::string& sElementPath, __in_z LPCWSTR wzValue);
HRESULT ValidateJsonSchema(__in_z LPCWSTR wzFile, __in_z LPCWSTR wzSchemaFile);

std::string GetLastErrorAsString();
HRESULT ReturnLastError(const std::string& action);
