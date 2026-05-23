#include "stdafx.h"
#include "JsonFile.h"
#include "jsoncons_ext/jsonschema/jsonschema.hpp"

HRESULT ValidateJsonSchema(__in_z LPCWSTR wzFile, __in_z LPCWSTR wzSchemaFile)
{
    // Validates the JSON file against the supplied JSON Schema using the jsoncons jsonschema
    // extension, which supports JSON Schema drafts 4/7/2019-09/2020-12 (selected by the schema's
    // $schema keyword, defaulting to the latest when absent).
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

        auto schema = jsoncons::jsonschema::make_json_schema(schemaData);
        if (!schema.is_valid(jsonData))
        {
            WcaLog(LOGMSG_STANDARD, "WixJsonFile: JSON file '%ls' failed validation against schema '%ls'", wzFile, wzSchemaFile);
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
        WcaLog(LOGMSG_STANDARD, "Schema validation error: %s", e.what());
        return E_FAIL;
    }
    catch (...)
    {
        WcaLog(LOGMSG_STANDARD, "encountered unknown error during schema validation");
        return E_FAIL;
    }
}
