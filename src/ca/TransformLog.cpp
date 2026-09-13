#include "stdafx.h"
#include "JsonFile.h"

// The transform log (JSONEXT_TRANSFORMLOG) is a JSON array with one entry per operation the
// deferred action processed. Entries are appended one at a time - read, push, write - so the
// log is complete up to the last operation even if the install is aborted or rolled back
// afterwards (the log itself is deliberately not part of the rollback). It is a diagnostic
// aid: a problem writing it is logged and otherwise ignored, it never fails an install.
HRESULT AppendTransformLogEntry(__in_z LPCWSTR wzLogFile, const json& entry)
{
    try
    {
        if (NULL == wzLogFile || L'\0' == *wzLogFile)
        {
            return E_INVALIDARG;
        }

        json entries = json::make_array();
        fs::path logPath(wzLogFile);
        if (fs::exists(logPath))
        {
            std::ifstream is{ logPath };
            if (is.is_open())
            {
                try
                {
                    json existing = json::parse(is);
                    if (existing.is_array())
                    {
                        entries = std::move(existing);
                    }
                    else
                    {
                        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Transform log '%ls' is not a JSON array; starting a new one", wzLogFile);
                    }
                }
                catch (const std::exception& e)
                {
                    WcaLog(LOGMSG_STANDARD, "WixJsonFile: Transform log '%ls' could not be parsed (%s); starting a new one", wzLogFile, e.what());
                }
            }
        }
        else
        {
            std::error_code ec;
            fs::path parent = logPath.parent_path();
            if (!parent.empty() && !fs::exists(parent))
            {
                fs::create_directories(parent, ec);
            }
        }

        entries.push_back(entry);

        // WriteJsonOutput falls back to a move when the target does not exist yet.
        return WriteJsonOutput(wzLogFile, entries);
    }
    catch (const std::exception& e)
    {
        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Failed to append to transform log '%ls': %s", wzLogFile, e.what());
        return E_FAIL;
    }
    catch (...)
    {
        WcaLog(LOGMSG_STANDARD, "WixJsonFile: Unknown error appending to transform log '%ls'", wzLogFile);
        return E_FAIL;
    }
}
