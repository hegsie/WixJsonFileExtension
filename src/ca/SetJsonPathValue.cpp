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

        _bstr_t bFile(wzFile);
        char* cFile = bFile;

        _bstr_t bValue(wzValue);
        char* cValue = bValue;
        HRESULT hr = S_OK;

        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Checking if file '%ls' exists", wzFile);
        if (fs::exists(fs::path(wzFile))) {

            std::ifstream is(cFile);
            WcaLog(LOGMSG_VERBOSE, "WixJsonFile: Opened file '%ls'", wzFile);

            if (!is.is_open())
            {
                WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Failed to open file stream for '%ls'", wzFile);
                hr = ReturnLastError("Opening the file stream");
                if (FAILED(hr)) return hr;
            }

            json j = json::parse(is);
            is.close();
            WcaLog(LOGMSG_VERBOSE, "WixJsonFile: Successfully parsed JSON file '%ls'", wzFile);

            if (createValue) {
                std::error_code ec;
                jsonpointer::add_if_absent(j, sElementPath, json(cValue), ec);

                if (ec) {
                    WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - JSONPointer add_if_absent failed for path '%s' in file '%ls': %s", 
                           sElementPath.c_str(), wzFile, ec.message().c_str());
                    return E_FAIL;
                }
                else {
                    std::ofstream os(wzFile,
                        std::ios_base::out | std::ios_base::trunc);

                    if (!os.is_open())
                    {
                        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Failed to create output stream for file '%ls'", wzFile);
                        hr = ReturnLastError("creating the output stream");
                        if (FAILED(hr)) return hr;
                    }

                    pretty_print(j).dump(os);
                    os.close();
                    WcaLog(LOGMSG_VERBOSE, "WixJsonFile: Successfully created path '%s' in file '%ls'", sElementPath.c_str(), wzFile);
                }
            }
            else {

                json query = jsonpath::json_query(j, sElementPath);

                WcaLog(LOGMSG_VERBOSE, "WixJsonFile: JSONPath query '%s' found %d element(s) in file '%ls'", 
                       sElementPath.c_str(), query.size(), wzFile);

                if (!query.empty()) {
                    auto f = [cValue](const std::string& /*path*/, json& value)
                        {
                            value = cValue;
                        };

                    jsonpath::json_replace(j, sElementPath, f);

                    hr = ReturnLastError("Replacing elements in the json");
                    if (FAILED(hr)) return hr;

                    WcaLog(LOGMSG_STANDARD, "WixJsonFile: Successfully updated path '%s' in file '%ls' with value '%s'", 
                           sElementPath.c_str(), wzFile, cValue);

                    std::ofstream os(wzFile, std::ios_base::out | std::ios_base::trunc);

                    if (!os.is_open())
                    {
                        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Failed to create output stream for file '%ls'", wzFile);
                        hr = ReturnLastError("creating the output stream");
                        if (FAILED(hr)) return hr;
                    }

                    pretty_print(j).dump(os);
                    os.close();
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
