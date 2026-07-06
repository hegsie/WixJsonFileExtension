#include "stdafx.h"
#include "JsonFile.h"

HRESULT SetJsonPathObject(__in_z LPCWSTR wzFile, const std::string& sElementPath, __in_z LPCWSTR wzValue) {

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

        if (NULL == wzValue || L'\0' == *wzValue)
        {
            WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Invalid value parameter for path '%s' in file '%ls'", 
                   sElementPath.c_str(), wzFile);
            return E_INVALIDARG;
        }

        HRESULT hr = S_OK;

        std::string valueUtf8;
        hr = WideToUtf8(wzValue, valueUtf8);
        if (FAILED(hr))
        {
            WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Failed to convert value to UTF-8 for path '%s' in file '%ls' (hr=0x%08X)", sElementPath.c_str(), wzFile, static_cast<unsigned int>(hr));
            return hr;
        }

        if (fs::exists(fs::path(wzFile))) {
            json j;
            SetLastError(0);
            std::ifstream is{ fs::path(wzFile) };

            if (!is.is_open())
            {
                WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Failed to open file stream for '%ls'", wzFile);
                hr = ReturnLastError("Opening the file stream");
                return FAILED(hr) ? hr : HRESULT_FROM_WIN32(ERROR_OPEN_FAILED);
            }

            is >> j;
            is.close();

            json obj;
            try {
                obj = json::parse(valueUtf8);
                WcaLog(LOGMSG_VERBOSE, "WixJsonFile: Parsed replacement JSON value for path '%s'", sElementPath.c_str());
            }
            catch (const std::exception& e) {
                WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Failed to parse JSON value for path '%s' in file '%ls': %s", 
                       sElementPath.c_str(), wzFile, e.what());
                return E_FAIL;
            }

            auto query = jsonpath::json_query(j, sElementPath);

            if (query.empty())
            {
                WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - No elements found at path '%s' in file '%ls' to replace", 
                       sElementPath.c_str(), wzFile);
                return HRESULT_FROM_WIN32(ERROR_OBJECT_NOT_FOUND);
            }

            WcaLog(LOGMSG_VERBOSE, "WixJsonFile: Found %d element(s) at path '%s' in file '%ls' to replace", 
                   query.size(), sElementPath.c_str(), wzFile);

            auto f = [obj](const std::string& /*path*/, json& value)
                {
                    value = obj;
                };

            jsonpath::json_replace(j, sElementPath, f);

            WcaLog(LOGMSG_STANDARD, "WixJsonFile: Successfully replaced JSON object at path '%s' in file '%ls'", 
                   sElementPath.c_str(), wzFile);

            hr = WriteJsonOutput(wzFile, j);
            if (FAILED(hr))
            {
                return hr;
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
        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Encountered COM error while replacing JSON object in file '%ls': %ls", 
               wzFile, e.ErrorMessage());
        return E_FAIL;
    }
    catch (std::exception& e)
    {
        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Encountered exception while replacing JSON object in file '%ls': %s", 
               wzFile, e.what());
        return E_FAIL;
    }
    catch (...)
    {
        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Encountered unknown error while replacing JSON object in file '%ls'", wzFile);
        return E_FAIL;
    }
}
