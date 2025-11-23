#include "stdafx.h"
#include "JsonFile.h"

HRESULT ValidateJsonSchema(__in_z LPCWSTR wzFile, __in_z LPCWSTR wzSchemaFile)
{
    // Basic JSON Schema validation implementation
    // 
    // Supported features:
    //   - Root type validation
    //   - Required properties checking (for objects)
    //   - Property type validation
    //
    // Known limitations:
    //   - Integer types accept any numeric value (not validated for whole numbers)
    //   - No support for: $ref, patterns, enums, min/max, format validation, 
    //     conditional schemas, or nested validation beyond type checking
    //
    // For full JSON Schema Draft 7+ compliance, use a dedicated JSON Schema library.
    
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

        _bstr_t bFile(wzFile);
        char* cFile = bFile;

        _bstr_t bSchemaFile(wzSchemaFile);
        char* cSchemaFile = bSchemaFile;

        HRESULT hr = S_OK;

        // Check if both files exist
        if (!fs::exists(fs::path(wzFile)))
        {
            WcaLog(LOGMSG_STANDARD, "JSON file not found: %s", cFile);
            return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        }

        if (!fs::exists(fs::path(wzSchemaFile)))
        {
            WcaLog(LOGMSG_STANDARD, "Schema file not found: %s", cSchemaFile);
            return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        }

        WcaLog(LOGMSG_STANDARD, "Loading JSON file: %s", cFile);
        std::ifstream jsonIs(cFile);
        if (!jsonIs.is_open())
        {
            WcaLog(LOGMSG_STANDARD, "Failed to open JSON file: %s", cFile);
            return E_FAIL;
        }

        json jsonData = json::parse(jsonIs);
        jsonIs.close();

        WcaLog(LOGMSG_STANDARD, "Loading schema file: %s", cSchemaFile);
        std::ifstream schemaIs(cSchemaFile);
        if (!schemaIs.is_open())
        {
            WcaLog(LOGMSG_STANDARD, "Failed to open schema file: %s", cSchemaFile);
            return E_FAIL;
        }

        json schemaData = json::parse(schemaIs);
        schemaIs.close();

        WcaLog(LOGMSG_STANDARD, "Validating JSON against schema");

        // Basic schema validation implementation
        // Supported features:
        //   - Root type validation
        //   - Required properties checking (for objects)
        //   - Property type validation
        //   - Integer types treated as numbers (no whole number validation)
        //
        // Unsupported features (would require full JSON Schema library):
        //   - $ref references
        //   - Pattern validation
        //   - Enum validation
        //   - Min/max constraints
        //   - Array items validation beyond type
        //   - Nested object validation beyond type
        //   - Format validation
        //   - Conditional schemas (if/then/else)
        //
        // For full JSON Schema Draft 7+ compliance, consider using a dedicated library

        // Check if schema has "type" property
        if (schemaData.contains("type"))
        {
            if (!schemaData["type"].is_string())
            {
                WcaLog(LOGMSG_STANDARD, "Invalid schema: 'type' must be a string");
                return E_FAIL;
            }
            
            std::string expectedType = schemaData["type"].as<std::string>();
            std::string actualType;

            if (jsonData.is_object()) actualType = "object";
            else if (jsonData.is_array()) actualType = "array";
            else if (jsonData.is_string()) actualType = "string";
            else if (jsonData.is_number()) actualType = "number";
            else if (jsonData.is_bool()) actualType = "boolean";
            else if (jsonData.is_null()) actualType = "null";

            if (expectedType != actualType)
            {
                WcaLog(LOGMSG_STANDARD, "Type mismatch: expected %s but got %s",
                    expectedType.c_str(), actualType.c_str());
                return E_FAIL;
            }
        }

        // Check required properties for objects
        if (schemaData.contains("required") && jsonData.is_object())
        {
            auto required = schemaData["required"];
            if (required.is_array())
            {
                for (const auto& req : required.array_range())
                {
                    std::string propName = req.as<std::string>();
                    if (!jsonData.contains(propName))
                    {
                        WcaLog(LOGMSG_STANDARD, "Required property missing: %s", propName.c_str());
                        return E_FAIL;
                    }
                }
            }
        }

        // Check properties types
        if (schemaData.contains("properties") && jsonData.is_object())
        {
            auto properties = schemaData["properties"];
            for (const auto& prop : properties.object_range())
            {
                std::string propName = prop.key();
                if (jsonData.contains(propName))
                {
                    auto propSchema = prop.value();
                    if (propSchema.contains("type"))
                    {
                        std::string expectedType = propSchema["type"].as<std::string>();
                        auto& actualValue = jsonData[propName];

                        std::string actualType;
                        if (actualValue.is_object()) actualType = "object";
                        else if (actualValue.is_array()) actualType = "array";
                        else if (actualValue.is_string()) actualType = "string";
                        else if (actualValue.is_number()) actualType = "number";
                        else if (actualValue.is_bool()) actualType = "boolean";
                        else if (actualValue.is_null()) actualType = "null";

                        // Check type match - allow integer schema to match number values
                        bool typeMatches = (expectedType == actualType);
                        if (!typeMatches && expectedType == "integer" && actualType == "number")
                        {
                            // Allow numeric values for integer schema types (basic validation only)
                            // Limitation: This doesn't verify the number is a whole number.
                            // For strict integer validation, a full JSON Schema library would be needed.
                            typeMatches = true;
                        }

                        if (!typeMatches)
                        {
                            WcaLog(LOGMSG_STANDARD, "Type mismatch for property '%s': expected %s but got %s",
                                propName.c_str(), expectedType.c_str(), actualType.c_str());
                            return E_FAIL;
                        }
                    }
                }
            }
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
