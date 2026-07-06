#include "stdafx.h"
#include "JsonFile.h"

HRESULT SetJsonPathValue(__in_z LPCWSTR wzFile, const std::string& sElementPath, __in_z LPCWSTR wzValue, bool createValue) {

    try
    {
        // Input validation
        if (NULL == wzFile || L'\0' == *wzFile)
        {
            WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Invalid file path parameter");
            return E_INVALIDARG;
        }

        if (sElementPath.empty())
        {
            WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Invalid element path parameter for file '%ls'", wzFile);
            return E_INVALIDARG;
        }

        HRESULT hr = S_OK;
        std::string valueUtf8;

        // A NULL value is treated as an empty string (preserves prior behavior); a non-NULL
        // value is converted from UTF-16 to UTF-8 so non-ASCII characters survive the JSON write.
        if (NULL != wzValue)
        {
            hr = WideToUtf8(wzValue, valueUtf8);
            if (FAILED(hr))
            {
                WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Failed to convert value to UTF-8 for path '%s' in file '%ls' (hr=0x%08X)", sElementPath.c_str(), wzFile, static_cast<unsigned int>(hr));
                return hr;
            }
        }

        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Checking if file '%ls' exists", wzFile);
        if (fs::exists(fs::path(wzFile))) {

            SetLastError(0);
            std::ifstream is{ fs::path(wzFile) };

            if (!is.is_open())
            {
                WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Failed to open file stream for '%ls'", wzFile);
                hr = ReturnLastError("Opening the file stream");
                return FAILED(hr) ? hr : HRESULT_FROM_WIN32(ERROR_OPEN_FAILED);
            }

            WcaLog(LOGMSG_VERBOSE, "WixJsonFile: Opened file '%ls'", wzFile);

            json j = json::parse(is);
            is.close();
            WcaLog(LOGMSG_VERBOSE, "WixJsonFile: Successfully parsed JSON file '%ls'", wzFile);

            if (createValue) {
                std::error_code ec;

                // Preserve the string type when overwriting an existing string value; otherwise
                // parse the authored value so numbers/booleans/objects become typed JSON.
                const json* pExisting = NULL;
                std::error_code ecGet;
                const json& existing = jsonpointer::get(j, sElementPath, ecGet);
                if (!ecGet)
                {
                    pExisting = &existing;
                }

                // jsonpointer::add sets the value whether or not the path exists (insert_or_assign),
                // with create_if_missing=true so intermediate objects are created, allowing a nested
                // pointer (e.g. /Application/Name) to be built from an empty/partial document.
                jsonpointer::add(j, sElementPath, MakeJsonValue(valueUtf8, pExisting), true, ec);

                if (ec) {
                    WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - JSONPointer add failed for path '%s' in file '%ls': %s",
                           sElementPath.c_str(), wzFile, ec.message().c_str());
                    return E_FAIL;
                }

                hr = WriteJsonOutput(wzFile, j);
                if (FAILED(hr))
                {
                    return hr;
                }
                WcaLog(LOGMSG_VERBOSE, "WixJsonFile: Successfully set path '%s' in file '%ls'", sElementPath.c_str(), wzFile);
            }
            else {

                json query = jsonpath::json_query(j, sElementPath);

                WcaLog(LOGMSG_VERBOSE, "WixJsonFile: JSONPath query '%s' found %d element(s) in file '%ls'",
                       sElementPath.c_str(), query.size(), wzFile);

                if (!query.empty()) {
                    // Type-preserving update: existing string values stay strings; anything else
                    // takes the parsed (typed) form of the authored value with string fallback.
                    auto f = [valueUtf8](const std::string& /*path*/, json& value)
                        {
                            value = MakeJsonValue(valueUtf8, &value);
                        };

                    jsonpath::json_replace(j, sElementPath, f);

                    WcaLog(LOGMSG_STANDARD, "WixJsonFile: Successfully updated path '%s' in file '%ls' with value '%s'",
                           sElementPath.c_str(), wzFile, valueUtf8.c_str());

                    hr = WriteJsonOutput(wzFile, j);
                    if (FAILED(hr))
                    {
                        return hr;
                    }
                }
                else {
                    WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - No elements found at path '%s' in file '%ls'. Ensure the path exists or use createJsonPointerValue action to create it.",
                           sElementPath.c_str(), wzFile);

                    return HRESULT_FROM_WIN32(ERROR_OBJECT_NOT_FOUND);
                }
            }
        }
        else {
            WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Unable to locate file '%ls'. Verify the file exists and the path is correct.", wzFile);
            return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        }
        return S_OK;
    }
    catch (_com_error& e)
    {
        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Encountered COM error while processing file '%ls': %ls", wzFile, e.ErrorMessage());
        return E_FAIL;
    }
    catch (std::exception& e)
    {
        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Encountered exception while processing file '%ls': %s", wzFile, e.what());
        return E_FAIL;
    }
    catch (...)
    {
        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Encountered unknown error while processing file '%ls'", wzFile);
        return E_FAIL;
    }
}
