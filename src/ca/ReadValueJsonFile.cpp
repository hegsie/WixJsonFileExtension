#include "stdafx.h"
#include "JsonFile.h"

/******************************************************************
 * ReadValueJsonFile - entry point for JsonFile Custom Action
 *****************************************************************/
extern "C" UINT WINAPI ReadValueJsonFile(
    __in MSIHANDLE hInstall
)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    PMSIHANDLE hView = NULL;
    PMSIHANDLE hRec = NULL;

    JSON_FILE_CHANGE* pxfcHead = NULL;
    JSON_FILE_CHANGE* pxfcTail = NULL;
    JSON_FILE_CHANGE* pxfc = NULL;

    LPWSTR pwzCustomActionData = NULL;

    DWORD cFiles = 0;

    // initialize
    hr = WcaInitialize(hInstall, "ReadValueJsonFile");
    ExitOnFailure(hr, "failed to initialize")

    hr = ReadJsonFileTable(&pxfcHead, &pxfcTail);
    if (S_FALSE == hr)
    {
        WcaLog(LOGMSG_VERBOSE, "Skipping ReadValueJsonFile because WixJsonFile table not present");
        ExitFunction1(hr = S_OK)
    }

    MessageExitOnFailure(hr, msierrJsonFileFailedRead, "failed to read WixJsonFile table")

    WcaLog(LOGMSG_STANDARD, "Finished Reading WixJsonFile");
    // loop through all the json configurations
    for (pxfc = pxfcHead; pxfc; pxfc = pxfc->pxfcNext)
    {
        // If it's being installed
        if (WcaIsInstalling(pxfc->isInstalled, pxfc->isAction))
        {
            std::bitset<32> flags(pxfc->iJsonFlags);
            if (flags.test(FLAG_DELETEVALUE) ||
                flags.test(FLAG_SETVALUE) ||
                flags.test(FLAG_REPLACEJSONVALUE) ||
                flags.test(FLAG_CREATEVALUE))
            {
                continue;
            }
            else if (flags.test(FLAG_READVALUE))
            {
                WcaLog(LOGMSG_STANDARD, "Updating property %ls from file %ls with path %ls", 
                    pxfc->pwzProperty, pxfc->wzFile, pxfc->pwzElementPath);

                if (fs::exists(fs::path(pxfc->wzFile)))
                {
                    try
                    {
                        std::ifstream is{ fs::path(pxfc->wzFile) };
                        auto fileJson = jsoncons::json::parse(is);
                        is.close();

                        WcaLog(LOGMSG_STANDARD, "Parsed File");

                        std::string elementPath;
                        HRESULT hrPath = WideToUtf8(pxfc->pwzElementPath, elementPath);
                        if (FAILED(hrPath))
                        {
                            WcaLog(LOGMSG_STANDARD, "Failed to convert element path to UTF-8 for property %ls (hr=0x%08X), setting default",
                                pxfc->pwzProperty, static_cast<unsigned int>(hrPath));
                            WcaSetProperty(pxfc->pwzProperty, pxfc->pwzDefaultValue);
                        }
                        else
                        {
                            jsoncons::json result = jsonpath::json_query(fileJson, elementPath);

                            WcaLog(LOGMSG_STANDARD, "Completed query of json file");

                            if (result.empty()) {
                                WcaLog(LOGMSG_STANDARD, "No results found for %s, setting default", elementPath.c_str());
                                WcaSetProperty(pxfc->pwzProperty, pxfc->pwzDefaultValue);
                            }
                            else {
                                WcaLog(LOGMSG_STANDARD, "Found %d results for %s", result.size(), elementPath.c_str());

                                // json_query returns an array of matches; use the first match.
                                jsoncons::json match = (result.is_array() && !result.empty()) ? result.at(0) : result;
                                std::string valueUtf8 = match.as<std::string>();

                                // jsoncons stores strings as UTF-8; convert back to UTF-16 so the MSI
                                // property preserves non-ASCII characters (CA2W would assume ANSI).
                                std::wstring wideValue;
                                HRESULT hrValue = Utf8ToWide(valueUtf8.c_str(), wideValue);
                                if (SUCCEEDED(hrValue))
                                {
                                    WcaSetProperty(pxfc->pwzProperty, wideValue.c_str());
                                }
                                else
                                {
                                    WcaLog(LOGMSG_STANDARD, "Failed to convert value to UTF-16 for property %ls (hr=0x%08X), setting default",
                                        pxfc->pwzProperty, static_cast<unsigned int>(hrValue));
                                    WcaSetProperty(pxfc->pwzProperty, pxfc->pwzDefaultValue);
                                }
                            }
                        }
                    }
                    catch (const std::exception& e)
                    {
                        WcaLog(LOGMSG_STANDARD, "Failed to read value for property %ls: %s. Setting default.",
                            pxfc->pwzProperty, e.what());
                        WcaSetProperty(pxfc->pwzProperty, pxfc->pwzDefaultValue);
                    }
                }
                else
                {
                    WcaLog(LOGMSG_STANDARD, "File %ls not found", pxfc->wzFile);
                }
            }
            ++cFiles;
        }
    }

    WcaLog(LOGMSG_STANDARD, "Built File list!");

    // If we looped through all records all is well
    if (E_NOMOREITEMS == hr)
        hr = S_OK;
    ExitOnFailure(hr, "failed while looping through all objects to secure")

LExit:
    // Free the linked list to prevent memory leak
    if (pxfcHead)
    {
        FreeJsonFileChangeList(pxfcHead);
    }

    return WcaFinalize(FAILED(hr) ? ERROR_INSTALL_FAILURE : er);
}
