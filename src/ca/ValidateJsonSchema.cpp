#include "stdafx.h"
#include "JsonFile.h"
#include "jsoncons_ext/jsonschema/jsonschema.hpp"

namespace jsonschema = jsoncons::jsonschema;

// Validates a JSON file against a JSON Schema with jsoncons' validator: drafts 4, 6, 7, 2019-09
// and 2020-12 (chosen by the schema's "$schema", 2020-12 when absent), including nested
// properties, items, required, enum, pattern, min/max, format, $ref within the document, and
// the conditional/combinator keywords. Every violation is logged with its instance location
// before the validation is reported as failed, so a log shows what to fix rather than only
// that something is wrong.
HRESULT ValidateJsonSchema(__in_z LPCWSTR wzFile, __in_z LPCWSTR wzSchemaFile)
{
    try
    {
        // Input validation
        if (NULL == wzFile || L'\0' == *wzFile)
        {
            WcaLog(LOGMSG_STANDARD, "Invalid file path parameter");
            return E_INVALIDARG;
        }

        if (NULL == wzSchemaFile || L'\0' == *wzSchemaFile)
        {
            WcaLog(LOGMSG_STANDARD, "Invalid schema file path parameter");
            return E_INVALIDARG;
        }

        // Check if both files exist
        if (!fs::exists(fs::path(wzFile)))
        {
            WcaLog(LOGMSG_STANDARD, "JSON file not found: %ls", wzFile);
            return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        }

        if (!fs::exists(fs::path(wzSchemaFile)))
        {
            WcaLog(LOGMSG_STANDARD, "Schema file not found: %ls", wzSchemaFile);
            return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        }

        WcaLog(LOGMSG_STANDARD, "Loading JSON file: %ls", wzFile);
        std::ifstream jsonIs{ fs::path(wzFile) };
        if (!jsonIs.is_open())
        {
            WcaLog(LOGMSG_STANDARD, "Failed to open JSON file: %ls", wzFile);
            return HRESULT_FROM_WIN32(ERROR_OPEN_FAILED);
        }

        json jsonData = json::parse(jsonIs);
        jsonIs.close();

        WcaLog(LOGMSG_STANDARD, "Loading schema file: %ls", wzSchemaFile);
        std::ifstream schemaIs{ fs::path(wzSchemaFile) };
        if (!schemaIs.is_open())
        {
            WcaLog(LOGMSG_STANDARD, "Failed to open schema file: %ls", wzSchemaFile);
            return HRESULT_FROM_WIN32(ERROR_OPEN_FAILED);
        }

        json schemaData = json::parse(schemaIs);
        schemaIs.close();

        WcaLog(LOGMSG_STANDARD, "Validating JSON against schema");

        jsonschema::json_schema<json> compiled = jsonschema::make_json_schema(std::move(schemaData));

        const size_t cMaxLogged = 25;
        size_t cErrors = 0;
        auto reporter = [&](const jsonschema::validation_message& msg) -> jsonschema::walk_result
        {
            ++cErrors;
            if (cErrors <= cMaxLogged)
            {
                std::string where = msg.instance_location().string();
                WcaLog(LOGMSG_STANDARD, "Schema violation at '%s' (%s): %s",
                    where.empty() ? "/" : where.c_str(), msg.keyword().c_str(), msg.message().c_str());
            }
            return jsonschema::walk_result::advance;
        };
        compiled.validate(jsonData, reporter);

        if (0 != cErrors)
        {
            if (cErrors > cMaxLogged)
            {
                WcaLog(LOGMSG_STANDARD, "... and %u more schema violation(s)", static_cast<unsigned int>(cErrors - cMaxLogged));
            }
            WcaLog(LOGMSG_STANDARD, "JSON schema validation failed with %u violation(s)", static_cast<unsigned int>(cErrors));
            return E_FAIL;
        }

        WcaLog(LOGMSG_STANDARD, "JSON schema validation successful");
        return S_OK;
    }
    catch (_com_error& e)
    {
        WcaLog(LOGMSG_STANDARD, "encountered COM error: %ls", e.ErrorMessage());
        return E_FAIL;
    }
    catch (std::exception& e)
    {
        // Also reached for a schema that does not compile (unknown draft, bad $ref, ...).
        WcaLog(LOGMSG_STANDARD, "Schema validation error: %s", e.what());
        return E_FAIL;
    }
    catch (...)
    {
        WcaLog(LOGMSG_STANDARD, "encountered unknown error during schema validation");
        return E_FAIL;
    }
}
