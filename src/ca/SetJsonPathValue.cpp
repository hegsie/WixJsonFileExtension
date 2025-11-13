#include "stdafx.h"
#include "JsonFile.h"

HRESULT SetJsonPathValue(__in_z LPCWSTR wzFile, const std::string& sElementPath, __in_z LPCWSTR wzValue, bool createValue) {

    try
    {
        // Input validation
        if (NULL == wzFile || L'\0' == *wzFile)
        {
            WcaLog(LOGMSG_STANDARD, "Invalid file path parameter");
            return E_INVALIDARG;
        }

        if (sElementPath.empty())
        {
            WcaLog(LOGMSG_STANDARD, "Invalid element path parameter");
            return E_INVALIDARG;
        }

        _bstr_t bFile(wzFile);
        char* cFile = bFile;

        _bstr_t bValue(wzValue);
        char* cValue = bValue;
        HRESULT hr = S_OK;

        WcaLog(LOGMSG_STANDARD, "Checking if %ls Exists", wzFile);
        if (fs::exists(fs::path(wzFile))) {

            std::ifstream is(cFile);
            WcaLog(LOGMSG_STANDARD, "Opened %s", cFile);

            if (!is.is_open())
            {
                hr = ReturnLastError("Opening the file stream");
                if (FAILED(hr)) return hr;
            }

            json j = json::parse(is);
            is.close();
            WcaLog(LOGMSG_STANDARD, "Parsed File");

            if (createValue) {
                std::error_code ec;
                jsonpointer::add_if_absent(j, sElementPath, json(cValue), ec);

                if (ec) {
                    WcaLog(LOGMSG_STANDARD, "json pointer add_if_absent %s", ec.message().c_str());
                    return E_FAIL;
                }
                else {
                    std::ofstream os(wzFile,
                        std::ios_base::out | std::ios_base::trunc);

                    if (!os.is_open())
                    {
                        hr = ReturnLastError("creating the output stream");
                        if (FAILED(hr)) return hr;
                    }

                    pretty_print(j).dump(os);
                    os.close();
                }
            }
            else {

                json query = jsonpath::json_query(j, sElementPath);

                WcaLog(LOGMSG_STANDARD, "Found %d elements", query.size());

                if (!query.empty()) {
                    auto f = [cValue](const std::string& /*path*/, json& value)
                        {
                            value = cValue;
                        };

                    jsonpath::json_replace(j, sElementPath, f);

                    hr = ReturnLastError("Replacing elements in the json");
                    if (FAILED(hr)) return hr;

                    WcaLog(LOGMSG_STANDARD, "Updating the json %s with values %s.", sElementPath.c_str(), cValue);

                    std::ofstream os(wzFile, std::ios_base::out | std::ios_base::trunc);

                    if (!os.is_open())
                    {
                        hr = ReturnLastError("creating the output stream");
                        if (FAILED(hr)) return hr;
                    }

                    pretty_print(j).dump(os);
                    os.close();
                }
                else {
                    WcaLog(LOGMSG_STANDARD, "No elements to update: %s", sElementPath.c_str());

                    return HRESULT_FROM_WIN32(ERROR_OBJECT_NOT_FOUND);
                }
            }
        }
        else {
            WcaLog(LOGMSG_STANDARD, "Unable to locate file: %s", cFile);
            return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        }
        return S_OK;
    }
    catch (std::exception& e)
    {
        WcaLog(LOGMSG_STANDARD, "encountered error %s", e.what());
        return E_FAIL;
    }
}
