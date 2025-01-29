#pragma once
#include "stdafx.h"

#include <atlbase.h>

using namespace jsoncons;
namespace fs = std::filesystem;

enum eJsonFileQuery { jfqId = 1, jfqFile, jfqElementPath, jfqValue, jfqDefaultValue, jfqFlags, jfqComponent, jfqProperty, jfqCompAttributes };


// These are bit positions
const int FLAG_DELETEVALUE = 0;
const int FLAG_SETVALUE = 1;
const int FLAG_REPLACEJSONVALUE = 2;
const int FLAG_CREATEVALUE = 3;
const int FLAG_READVALUE = 4;

// These are bits
enum eXmlAction
{
    jaDeleteValue = 1,
    jaSetValue = 2,
    jaReplaceJsonValue = 4,
    jaCreateJsonPointerValue = 8,
    jaReadValue = 16,
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

    JSON_FILE_CHANGE* pxfcPrev;
    JSON_FILE_CHANGE* pxfcNext;
};

HRESULT ReadJsonFileTable(
    __inout JSON_FILE_CHANGE** ppxfcHead,
    __inout JSON_FILE_CHANGE** ppxfcTail
);
HRESULT UpdateJsonFile(
    __in_z LPCWSTR wzFile,
    __in_z LPCWSTR wzElementPath,
    __in_z LPCWSTR wzValue,
    __in int iFlags
);
HRESULT SetJsonPathValue(__in_z LPCWSTR wzFile, const std::string& sElementPath, __in_z LPCWSTR wzValue, bool createValue);
HRESULT SetJsonPathObject(__in_z LPCWSTR wzFile, const std::string& sElementPath, __in_z LPCWSTR wzValue);
HRESULT DeleteJsonPath(__in_z LPCWSTR wzFile, std::string sElementPath);

std::string GetLastErrorAsString();
HRESULT ReturnLastError(std::string action);
