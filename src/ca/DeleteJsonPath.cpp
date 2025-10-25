#include "stdafx.h"
#include "JsonFile.h"

HRESULT DeleteJsonPath(__in_z LPCWSTR wzFile, std::string sElementPath)
{
    try
    {
        _bstr_t bFile(wzFile);
        char* cFile = bFile;
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

            auto expr = jsonpath::make_expression<json>(sElementPath);
            std::vector<jsonpath::json_location> locations = expr.select_paths(j,
                jsonpath::result_options::sort_descending | jsonpath::result_options::sort_descending);

            for (const auto& location : locations)
            {
                jsonpath::remove(j, location);
            }

            std::cout << j << '\n';

            WcaLog(LOGMSG_STANDARD, "Deleted the json %s", sElementPath.c_str());

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
        }
        return S_OK;
    }
    catch (std::exception& e)
    {
        WcaLog(LOGMSG_STANDARD, "encountered error %s", e.what());
        throw;
    }
}

