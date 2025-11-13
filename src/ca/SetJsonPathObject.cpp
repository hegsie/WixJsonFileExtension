#include "stdafx.h"
#include "JsonFile.h"

HRESULT SetJsonPathObject(__in_z LPCWSTR wzFile, const std::string& sElementPath, __in_z LPCWSTR wzValue) {

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

            std::string s = cValue;
            json obj = json::parse(s);

            WcaLog(LOGMSG_STANDARD, "Parsed the new value: $%s$", obj.to_string().c_str());

            auto query = jsonpath::json_query(j, sElementPath);

            WcaLog(LOGMSG_STANDARD, "About to update the json %s with values %s.", sElementPath.c_str(), query.as_string().c_str());

            auto f = [obj](const std::string& /*path*/, json& value)
                {
                    value = obj;
                };

            jsonpath::json_replace(j, sElementPath, f);

            WcaLog(LOGMSG_STANDARD, "Updated the json %s with values %s.", sElementPath.c_str(), obj.as_string().c_str());

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
