#include "stdafx.h"
#include "JsonFile.h"

HRESULT DeleteJsonPath(__in_z LPCWSTR wzFile, const std::string& sElementPath)
{
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

            auto expr = jsonpath::make_expression<json>(sElementPath);
            std::vector<jsonpath::json_location> locations = expr.select_paths(j,
                jsonpath::result_options::sort_descending);

            if (locations.empty())
            {
                WcaLog(LOGMSG_STANDARD, "WixJsonFile: Warning - No elements found at path '%s' in file '%ls' to delete", 
                       sElementPath.c_str(), wzFile);
            }
            else
            {
                for (const auto& location : locations)
                {
                    jsonpath::remove(j, location);
                }

                WcaLog(LOGMSG_STANDARD, "WixJsonFile: Successfully deleted %d element(s) at path '%s' in file '%ls'", 
                       locations.size(), sElementPath.c_str(), wzFile);
            }

            // Serialize before truncating so a serialization failure cannot leave the file empty.
            std::ostringstream serialized;
            serialized << pretty_print(j);

            SetLastError(0);
            std::ofstream os(wzFile, std::ios_base::out | std::ios_base::trunc);
            if (!os.is_open())
            {
                WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Failed to create output stream for file '%ls'", wzFile);
                hr = ReturnLastError("creating the output stream");
                return FAILED(hr) ? hr : HRESULT_FROM_WIN32(ERROR_OPEN_FAILED);
            }

            os << serialized.str();
            os.close();
            if (os.fail())
            {
                WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Failed to write file '%ls'", wzFile);
                return HRESULT_FROM_WIN32(ERROR_WRITE_FAULT);
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
        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Encountered COM error while deleting from file '%ls': %ls", wzFile, e.ErrorMessage());
        return E_FAIL;
    }
    catch (std::exception& e)
    {
        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Encountered exception while deleting from file '%ls': %s", wzFile, e.what());
        return E_FAIL;
    }
    catch (...)
    {
        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Error - Encountered unknown error while deleting from file '%ls'", wzFile);
        return E_FAIL;
    }
}

