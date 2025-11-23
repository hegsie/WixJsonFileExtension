#include "stdafx.h"
#include "JsonFile.h"

LPCWSTR vcsJsonFileQuery = L"SELECT `WixJsonFile`.`JsonConfig`, `WixJsonFile`.`File`, `WixJsonFile`.`ElementPath`, "
                           L"`WixJsonFile`.`Value`, `WixJsonFile`.`DefaultValue`, `WixJsonFile`.`Flags`, `WixJsonFile`.`Component_`, `WixJsonFile`.`Property`, `Component`.`Attributes`, `WixJsonFile`.`Index`, `WixJsonFile`.`SchemaFile` FROM `WixJsonFile`,`Component`"
                           L"WHERE `WixJsonFile`.`Component_`=`Component`.`Component` ORDER BY `File`, `Sequence`";

static HRESULT AddJsonFileChangeToList(
    __inout JSON_FILE_CHANGE **ppxfcHead,
    __inout JSON_FILE_CHANGE **ppxfcTail)
{
    Assert(ppxfcHead && ppxfcTail);

    HRESULT hr = S_OK;

    JSON_FILE_CHANGE *pxfc = static_cast<JSON_FILE_CHANGE *>(MemAlloc(sizeof(JSON_FILE_CHANGE), TRUE));
    ExitOnNull(pxfc, hr, E_OUTOFMEMORY, "failed to allocate memory for new xml file change list element")

    // Add it to the end of the list
    if (NULL == *ppxfcHead)
    {
        *ppxfcHead = pxfc;
        *ppxfcTail = pxfc;
    }
    else
    {
        Assert(*ppxfcTail && (*ppxfcTail)->pxfcNext == NULL);
        (*ppxfcTail)->pxfcNext = pxfc;
        pxfc->pxfcPrev = *ppxfcTail;
        *ppxfcTail = pxfc;
    }

LExit:
    return hr;
}

HRESULT ReadJsonFileTable(
    __inout JSON_FILE_CHANGE **ppxfcHead,
    __inout JSON_FILE_CHANGE **ppxfcTail)
{
    Assert(ppxfcHead && ppxfcTail);

    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    PMSIHANDLE hView = NULL;
    PMSIHANDLE hRec = NULL;

    LPWSTR pwzData = NULL;

    // check to see if necessary tables are specified
    if (S_FALSE == WcaTableExists(L"WixJsonFile"))
    {
        ExitFunction1(hr = S_FALSE)
    }

    // loop through all the xml configurations
    hr = WcaOpenExecuteView(vcsJsonFileQuery, &hView);
    ExitOnFailure(hr, "failed to open view on WixJsonFile table")

    while (S_OK == (hr = WcaFetchRecord(hView, &hRec)))
    {
        hr = AddJsonFileChangeToList(ppxfcHead, ppxfcTail);
        ExitOnFailure(hr, "failed to add xml file change to list")

        // Get record Id
        hr = WcaGetRecordString(hRec, jfqId, &pwzData);
        ExitOnFailure(hr, "failed to get WixJsonFile record Id")
        hr = StringCchCopyW((*ppxfcTail)->wzId, std::size((*ppxfcTail)->wzId), pwzData);
        ExitOnFailure(hr, "failed to copy WixJsonFile record Id")

        // Get component name
        hr = WcaGetRecordString(hRec, jfqComponent, &pwzData);
        ExitOnFailure(hr, "failed to get component name for WixJsonFile: %ls", (*ppxfcTail)->wzId)

        // Get the component's state
        er = ::MsiGetComponentStateW(WcaGetInstallHandle(), pwzData, &(*ppxfcTail)->isInstalled, &(*ppxfcTail)->isAction);
        ExitOnFailure(hr = HRESULT_FROM_WIN32(er), "failed to get install state for Component: %ls", pwzData)

        // Get the json file
        hr = WcaGetRecordFormattedString(hRec, jfqFile, &pwzData);
        ExitOnFailure(hr, "failed to get xml file for WixJsonFile: %ls", (*ppxfcTail)->wzId)
        hr = StringCchCopyW((*ppxfcTail)->wzFile, std::size((*ppxfcTail)->wzFile), pwzData);
        ExitOnFailure(hr, "failed to copy xml file path")

        // Get the table flags
        hr = WcaGetRecordInteger(hRec, jfqFlags, &(*ppxfcTail)->iJsonFlags);
        ExitOnFailure(hr, "failed to get WixJsonFile flags for WixJsonFile: %ls", (*ppxfcTail)->wzId)

        // Get the element path
        hr = WcaGetRecordFormattedString(hRec, jfqElementPath, &(*ppxfcTail)->pwzElementPath);
        ExitOnFailure(hr, "failed to get XPath for WixJsonFile: %ls", (*ppxfcTail)->wzId)

        // Get the value
        hr = WcaGetRecordFormattedString(hRec, jfqValue, &pwzData);
        ExitOnFailure(hr, "failed to get Value for WixJsonFile: %ls", (*ppxfcTail)->wzId)
        hr = StrAllocString(&(*ppxfcTail)->pwzValue, pwzData, 0);
        ExitOnFailure(hr, "failed to allocate buffer for value")

        // Get the default value
        hr = WcaGetRecordFormattedString(hRec, jfqDefaultValue, &pwzData);
        ExitOnFailure(hr, "failed to get Default Value for WixJsonFile: %ls", (*ppxfcTail)->wzId)
        hr = StrAllocString(&(*ppxfcTail)->pwzDefaultValue, pwzData, 0);
        ExitOnFailure(hr, "failed to allocate buffer for default value")

        // Get the component attributes
        hr = WcaGetRecordInteger(hRec, jfqCompAttributes, &(*ppxfcTail)->iCompAttributes);
        ExitOnFailure(hr, "failed to get component attributes for WixJsonFile: %ls", (*ppxfcTail)->wzId)

        // Get the default value
        hr = WcaGetRecordFormattedString(hRec, jfqProperty, &pwzData);
        ExitOnFailure(hr, "failed to get Property for WixJsonFile: %ls", (*ppxfcTail)->wzId)
        hr = StrAllocString(&(*ppxfcTail)->pwzProperty, pwzData, 0);
        ExitOnFailure(hr, "failed to allocate buffer for property")

        // Get the index
        hr = WcaGetRecordInteger(hRec, jfqIndex, &(*ppxfcTail)->iIndex);
        if (FAILED(hr))
        {
            // Index is optional, default to -1
            (*ppxfcTail)->iIndex = -1;
            hr = S_OK;
        }

        // Get the schema file
        hr = WcaGetRecordFormattedString(hRec, jfqSchemaFile, &pwzData);
        ExitOnFailure(hr, "failed to get SchemaFile for WixJsonFile: %ls", (*ppxfcTail)->wzId)
        hr = StrAllocString(&(*ppxfcTail)->pwzSchemaFile, pwzData, 0);
        ExitOnFailure(hr, "failed to allocate buffer for schema file")
    }

    // if we looped through all records all is well
    if (E_NOMOREITEMS == hr)
        hr = S_OK;
    ExitOnFailure(hr, "failed while looping through all objects to secure")

LExit:
    ReleaseStr(pwzData)

    return hr;
}

void FreeJsonFileChangeList(
    __in JSON_FILE_CHANGE* pxfcHead)
{
    JSON_FILE_CHANGE* pxfc = pxfcHead;
    JSON_FILE_CHANGE* pxfcNext = NULL;

    while (pxfc)
    {
        pxfcNext = pxfc->pxfcNext;

        // Free dynamically allocated strings
        ReleaseStr(pxfc->pwzElementPath);
        ReleaseStr(pxfc->pwzValue);
        ReleaseStr(pxfc->pwzDefaultValue);
        ReleaseStr(pxfc->pwzProperty);
        ReleaseStr(pxfc->pwzSchemaFile);

        // Free the structure itself
        MemFree(pxfc);

        pxfc = pxfcNext;
    }
}
