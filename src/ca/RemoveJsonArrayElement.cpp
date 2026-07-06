#include "stdafx.h"
#include "JsonFile.h"

HRESULT RemoveJsonArrayElement(__in_z LPCWSTR wzFile, const std::string& sElementPath, __in_z LPCWSTR wzValue)
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

        HRESULT hr = S_OK;

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

            WcaLog(LOGMSG_STANDARD, "Removing elements from array at: %s", sElementPath.c_str());

            // If wzValue is provided, it should be a JSON value to match and remove
            // Otherwise, the path should point to specific elements to remove
            if (wzValue != NULL && L'\0' != *wzValue)
            {
                std::string valueUtf8;
                hr = WideToUtf8(wzValue, valueUtf8);
                if (FAILED(hr))
                {
                    WcaLog(LOGMSG_STANDARD, "Failed to convert value to UTF-8 for path '%s' in file '%ls' (hr=0x%08X)", sElementPath.c_str(), wzFile, static_cast<unsigned int>(hr));
                    return hr;
                }

                // Parse the value to match
                json valueToMatch;
                try {
                    valueToMatch = json::parse(valueUtf8);
                }
                catch (const std::exception&) {
                    // If parsing fails, treat as a string value
                    valueToMatch = json(valueUtf8);
                }

                // Find and remove matching elements
                auto f = [valueToMatch](const std::string& /*path*/, json& value)
                    {
                        if (value.is_array())
                        {
                            // Remove all elements that match the value
                            auto it = value.array_range().begin();
                            while (it != value.array_range().end())
                            {
                                if (*it == valueToMatch)
                                {
                                    it = value.erase(it);
                                }
                                else
                                {
                                    ++it;
                                }
                            }
                        }
                    };

                // Get parent array path (remove the filter part)
                std::string arrayPath = sElementPath;
                size_t filterPos = arrayPath.find("[?");
                if (filterPos != std::string::npos)
                {
                    arrayPath = arrayPath.substr(0, filterPos);
                }

                jsonpath::json_replace(j, arrayPath, f);
            }
            else
            {
                // Remove elements directly using the path (with filters or indices)
                auto expr = jsonpath::make_expression<json>(sElementPath);
                std::vector<jsonpath::json_location> locations = expr.select_paths(j,
                    jsonpath::result_options::sort_descending);

                for (const auto& location : locations)
                {
                    jsonpath::remove(j, location);
                }
            }

            WcaLog(LOGMSG_STANDARD, "Successfully removed elements from array");

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
