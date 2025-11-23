#include "stdafx.h"
#include "JsonFile.h"
#include <set>
#include <algorithm>

HRESULT DistinctJsonArray(__in_z LPCWSTR wzFile, const std::string& sElementPath)
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

        _bstr_t bFile(wzFile);
        char* cFile = bFile;
        HRESULT hr = S_OK;

        if (fs::exists(fs::path(wzFile))) {
            json j;
            std::ifstream is(cFile);

            if (!is.is_open())
            {
                WcaLog(LOGMSG_STANDARD, "Failed to open file for reading: %s", cFile);
                return HRESULT_FROM_WIN32(ERROR_OPEN_FAILED);
            }

            // Parse JSON with error handling
            try {
                is >> j;
            }
            catch (const std::exception& e) {
                is.close();
                WcaLog(LOGMSG_STANDARD, "Failed to parse JSON file: %s. Error: %s", cFile, e.what());
                return E_FAIL;
            }
            is.close();

            // Query the array using JSONPath
            auto query = jsonpath::json_query(j, sElementPath);

            if (query.empty())
            {
                WcaLog(LOGMSG_STANDARD, "Array not found at path: %s", sElementPath.c_str());
                return HRESULT_FROM_WIN32(ERROR_OBJECT_NOT_FOUND);
            }

            // Validate that all matched nodes are arrays
            bool allArrays = true;
            for (const auto& node : query)
            {
                if (!node.is_array())
                {
                    WcaLog(LOGMSG_STANDARD, "distinctValues action requires path to point to an array. Path: %s", sElementPath.c_str());
                    allArrays = false;
                    break;
                }
            }

            if (!allArrays)
            {
                return E_INVALIDARG;
            }

            WcaLog(LOGMSG_STANDARD, "Removing duplicates from array at: %s", sElementPath.c_str());

            // Remove duplicates from the array
            auto f = [](const std::string& /*path*/, json& value)
                {
                    if (value.is_array())
                    {
                        // Use a vector to track unique items
                        std::vector<json> uniqueItems;
                        std::set<std::string> seenStrings;

                        for (const auto& item : value.array_range())
                        {
                            // Serialize the item to string for comparison
                            std::string itemStr = item.to_string();
                            
                            // Only add if we haven't seen this string representation before
                            if (seenStrings.find(itemStr) == seenStrings.end())
                            {
                                uniqueItems.push_back(item);
                                seenStrings.insert(itemStr);
                            }
                        }

                        // Replace the array with unique items
                        value = json(uniqueItems.begin(), uniqueItems.end());
                    }
                };

            jsonpath::json_replace(j, sElementPath, f);

            WcaLog(LOGMSG_STANDARD, "Successfully removed duplicates from array");

            std::ofstream os(wzFile, std::ios_base::out | std::ios_base::trunc);

            if (!os.is_open())
            {
                WcaLog(LOGMSG_STANDARD, "Failed to open output file stream: %ls", wzFile);
                return HRESULT_FROM_WIN32(ERROR_OPEN_FAILED);
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
