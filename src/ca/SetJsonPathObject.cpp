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
                WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Failed to open file stream for '%ls'", wzFile);
                hr = ReturnLastError("Opening the file stream");
                if (FAILED(hr)) return hr;
            }

            is >> j;
            is.close();

            std::string s = cValue;
            json obj;
            try {
                obj = json::parse(s);
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
