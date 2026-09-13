#include "stdafx.h"
#include "JsonFile.h"

HRESULT AppendJsonArray(__in_z LPCWSTR wzFile, const std::string& sElementPath, __in_z LPCWSTR wzValue, int iValueType, __in_z_opt LPCWSTR wzCulture)
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

        HRESULT hr = S_OK;

        std::string valueUtf8;
        hr = WideToUtf8(wzValue, valueUtf8);
        if (FAILED(hr))
        {
            WcaLog(LOGMSG_STANDARD, "Failed to convert value to UTF-8 for path '%s' in file '%ls' (hr=0x%08X)", sElementPath.c_str(), wzFile, static_cast<unsigned int>(hr));
            return hr;
        }

        if (fs::exists(fs::path(wzFile))) {
            json j;
            std::ifstream is{ fs::path(wzFile) };

            if (!is.is_open())
            {
                WcaLog(LOGMSG_STANDARD, "Failed to open file for reading: %ls", wzFile);
                return HRESULT_FROM_WIN32(ERROR_OPEN_FAILED);
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

            // Convert the value to append (ValueType=auto: JSON when it parses, else a string)
            json valueToAppend;
            std::string convError;
            if (!ConvertAuthoredValue(valueUtf8, iValueType, wzCulture, NULL, valueToAppend, convError))
            {
                WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Value for path '%s' in file '%ls' does not convert to ValueType %s: %s",
                       sElementPath.c_str(), wzFile, JsonValueTypeName(iValueType), convError.c_str());
                return E_INVALIDARG;
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

            hr = WriteJsonOutput(wzFile, j);
            if (FAILED(hr))
            {
                return hr;
            }
        }
        else {
            WcaLog(LOGMSG_STANDARD, "Unable to locate file: %ls", wzFile);
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
