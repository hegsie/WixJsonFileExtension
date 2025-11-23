#include "stdafx.h"
#include "JsonFile.h"

HRESULT AppendJsonArray(__in_z LPCWSTR wzFile, const std::string& sElementPath, __in_z LPCWSTR wzValue)
{
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

        if (NULL == wzValue || L'\0' == *wzValue)
        {
            WcaLog(LOGMSG_STANDARD, "Invalid value parameter");
            return E_INVALIDARG;
        }

        _bstr_t bFile(wzFile);
        char* cFile = bFile;

        _bstr_t bValue(wzValue);
        char* cValue = bValue;
        HRESULT hr = S_OK;

        if (fs::exists(fs::path(wzFile))) {
            json j;
            std::ifstream is(cFile);

            if (!is.is_open())
            {
                hr = ReturnLastError("Opening the file stream");
                if (FAILED(hr)) return hr;
            }

            is >> j;
            is.close();

            // Query the array using JSONPath
            auto query = jsonpath::json_query(j, sElementPath);

            if (query.empty())
            {
                WcaLog(LOGMSG_STANDARD, "Array not found at path: %s", sElementPath.c_str());
                return HRESULT_FROM_WIN32(ERROR_OBJECT_NOT_FOUND);
            }

            // Parse the value to append
            json valueToAppend;
            try {
                std::string s = cValue;
                valueToAppend = json::parse(s);
            }
            catch (const std::exception&) {
                // If parsing fails, treat as a string value
                valueToAppend = json(cValue);
            }

            WcaLog(LOGMSG_STANDARD, "Appending value to array at: %s", sElementPath.c_str());

            // Append to the array
            auto f = [valueToAppend](const std::string& /*path*/, json& value)
                {
                    if (value.is_array())
                    {
                        value.push_back(valueToAppend);
                    }
                };

            jsonpath::json_replace(j, sElementPath, f);

            WcaLog(LOGMSG_STANDARD, "Successfully appended value to array");

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
            WcaLog(LOGMSG_STANDARD, "Unable to locate file: %s", cFile);
            return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        }
        return S_OK;
    }
    catch (_com_error& e)
    {
        WcaLog(LOGMSG_STANDARD, "encountered COM error: %ls", e.ErrorMessage());
        return E_FAIL;
    }
    catch (std::exception& e)
    {
        WcaLog(LOGMSG_STANDARD, "encountered error %s", e.what());
        return E_FAIL;
    }
    catch (...)
    {
        WcaLog(LOGMSG_STANDARD, "encountered unknown error");
        return E_FAIL;
    }
}
