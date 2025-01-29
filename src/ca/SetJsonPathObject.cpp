#include "stdafx.h"
#include "JsonFile.h"

HRESULT SetJsonPathObject(__in_z LPCWSTR wzFile, std::string sElementPath, __in_z LPCWSTR wzValue) {

    try
    {
        _bstr_t bFile(wzFile);
        char* cFile = bFile;

        _bstr_t bValue(wzValue);
        char* cValue = bValue;
        HRESULT hr = S_OK;

        if (fs::exists(fs::path(wzFile))) {
            json j;
            std::ifstream is(cFile);

            is >> j;

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
        }
        return S_OK;
    }
    catch (std::exception& e)
    {
        WcaLog(LOGMSG_STANDARD, "encountered error %s", e.what());
        throw;
    }
}
