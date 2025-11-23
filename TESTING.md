# Testing Guide for WixJsonFileExtension

This document describes testing scenarios and best practices for the WixJsonFileExtension.

## Overview

The WixJsonFileExtension should handle various scenarios when modifying JSON files during MSI installation. This guide covers typical use cases and edge cases to test.

## Test Scenarios

### 1. Modifying Existing JSON Files

#### Valid JSON File Modification

**Scenario**: Update an existing value in a valid JSON file

**Test Setup**:
```json
{
  "ConnectionStrings": {
    "DefaultConnection": "Server=localhost;Database=Test;"
  }
}
```

**WiX Configuration**:
```xml
<Json:JsonFile 
  Id="UpdateConnection" 
  File="[#ConfigFile]" 
  ElementPath="$.ConnectionStrings.DefaultConnection" 
  Value="Server=production;Database=Prod;" 
  Action="setValue" />
```

**Expected Result**: The connection string is updated successfully

#### Nested Path Modification

**Scenario**: Modify a deeply nested value

**Test Setup**:
```json
{
  "Logging": {
    "LogLevel": {
      "Default": "Information",
      "Microsoft": "Warning"
    }
  }
}
```

**WiX Configuration**:
```xml
<Json:JsonFile 
  Id="UpdateLogLevel" 
  File="[#ConfigFile]" 
  ElementPath="$.Logging.LogLevel.Default" 
  Value="Debug" 
  Action="setValue" />
```

**Expected Result**: The log level is updated to "Debug"

### 2. Creating New JSON Values

#### Creating New Property with JSONPointer

**Scenario**: Create a new configuration section that doesn't exist

**Test Setup**:
```json
{
  "ExistingSection": {
    "Value": "test"
  }
}
```

**WiX Configuration**:
```xml
<Json:JsonFile 
  Id="CreateNewSection" 
  File="[#ConfigFile]" 
  ElementPath="/NewSection/NewValue" 
  Value="MyValue" 
  Action="createJsonPointerValue" />
```

**Expected Result**:
```json
{
  "ExistingSection": {
    "Value": "test"
  },
  "NewSection": {
    "NewValue": "MyValue"
  }
}
```

### 3. Handling Invalid JSON

#### Malformed JSON File

**Scenario**: Attempt to modify a file with invalid JSON syntax

**Test Setup** (invalid.json):
```json
{
  "InvalidJson": "Missing closing brace",
  "Data": {
    "Value": "test"
```

**WiX Configuration**:
```xml
<Json:JsonFile 
  Id="ModifyInvalid" 
  File="[#InvalidFile]" 
  ElementPath="$.Data.Value" 
  Value="NewValue" 
  Action="setValue" />
```

**Expected Result**: 
- The custom action should log an error
- The installation should continue (or fail gracefully based on configuration)
- The MSI log should contain a clear error message about the JSON parsing failure

### 4. Reading Values

#### Read Existing Value

**Scenario**: Read a value from JSON and store in a Windows Installer property

**Test Setup**:
```json
{
  "ApplicationSettings": {
    "Version": "1.0.0"
  }
}
```

**WiX Configuration**:
```xml
<Json:JsonFile 
  Id="ReadVersion" 
  File="[#ConfigFile]" 
  ElementPath="$.ApplicationSettings.Version" 
  Action="readValue" 
  Property="APP_VERSION" />
```

**Expected Result**: The property `APP_VERSION` contains "1.0.0"

#### Read Missing Value with Default

**Scenario**: Read a value that doesn't exist, using a default value

**Test Setup**:
```json
{
  "ApplicationSettings": {}
}
```

**WiX Configuration**:
```xml
<Json:JsonFile 
  Id="ReadMissing" 
  File="[#ConfigFile]" 
  ElementPath="$.ApplicationSettings.NonExistent" 
  DefaultValue="DefaultValue" 
  Action="readValue" 
  Property="MY_PROPERTY" />
```

**Expected Result**: The property `MY_PROPERTY` contains "DefaultValue"

### 5. Deleting Values

#### Delete Single Property

**Scenario**: Remove a specific property from JSON

**Test Setup**:
```json
{
  "Settings": {
    "TemporaryValue": "remove-me",
    "PermanentValue": "keep-me"
  }
}
```

**WiX Configuration**:
```xml
<Json:JsonFile 
  Id="DeleteTemp" 
  File="[#ConfigFile]" 
  ElementPath="$.Settings.TemporaryValue" 
  Action="deleteValue" />
```

**Expected Result**:
```json
{
  "Settings": {
    "PermanentValue": "keep-me"
  }
}
```

### 6. Common .NET Configuration Patterns

#### Connection Strings

**Scenario**: Configure database connection during installation

**Test**: Verify connection strings are properly set with installer properties

#### Logging Configuration

**Scenario**: Set logging levels based on environment

**Test**: Verify LogLevel values are correctly updated

#### Service Configuration

**Scenario**: Configure Windows Service settings

**Test**: Verify service name, display name, and paths are set correctly

### 7. Array Operations

#### Modify Array Element

**Scenario**: Update a specific element in a JSON array

**Test Setup**:
```json
{
  "Items": [
    {"Id": 1, "Name": "First"},
    {"Id": 2, "Name": "Second"}
  ]
}
```

**WiX Configuration**:
```xml
<Json:JsonFile 
  Id="UpdateArrayElement" 
  File="[#ConfigFile]" 
  ElementPath="$.Items[\[]0[\]].Name" 
  Value="Updated" 
  Action="setValue" />
```

**Expected Result**: First item's Name is "Updated"

### 8. Replace JSON Objects

#### Replace Entire Object

**Scenario**: Replace an entire JSON object with new content

**Test Setup**:
```json
{
  "OldObject": {
    "Property1": "value1"
  }
}
```

**WiX Configuration**:
```xml
<Property Id="NEW_JSON" Value='{"Property2": "value2", "Property3": "value3"}' />

<Json:JsonFile 
  Id="ReplaceObject" 
  File="[#ConfigFile]" 
  ElementPath="$.OldObject" 
  Value="[NEW_JSON]" 
  Action="replaceJsonValue" />
```

**Expected Result**:
```json
{
  "OldObject": {
    "Property2": "value2",
    "Property3": "value3"
  }
}
```

## Testing Best Practices

### 1. Test with Actual MSI Installation

Always test your changes by building and installing the actual MSI:

```bash
msbuild /p:Configuration=Release YourInstaller.wixproj
msiexec /i bin\Release\en-US\YourInstaller.msi /l*v install.log
```

Review the install.log file for any errors or warnings.

### 2. Verify JSON Structure After Installation

After installation, verify the JSON files are:
- Well-formed (valid JSON syntax)
- Contain the expected values
- Have the expected structure

### 3. Test Uninstall/Rollback

Test that changes are properly rolled back if installation fails or is rolled back.

### 4. Test with Different WiX Versions

If possible, test with:
- WiX 4.x
- WiX 5.x
- WiX 6.x

### 5. Test with Different .NET Application Types

Test with various application configurations:
- .NET 6+ applications with appsettings.json
- .NET Framework applications with custom JSON config
- ASP.NET Core web applications
- Console applications
- Windows Services

## Automated Testing with CI

The project includes automated regression tests in `.github/workflows/regression-tests.yml` that:

1. Build the solution
2. Build the test installer
3. Validate JSON syntax
4. Test common .NET patterns
5. Test JSONPath patterns

These tests run automatically on:
- Every push to main
- Every pull request
- Manual workflow dispatch

## Manual Test Checklist

Before submitting changes, verify:

- [ ] Solution builds without errors
- [ ] No new compiler warnings
- [ ] Test installer builds successfully
- [ ] Test MSI installs without errors
- [ ] JSON files are modified as expected
- [ ] Values from installer properties are correctly substituted
- [ ] Invalid JSON is handled gracefully
- [ ] MSI log contains appropriate messages
- [ ] Uninstall works correctly
- [ ] Existing functionality is not broken

## Reporting Test Results

When reporting issues or test results, include:

1. **Environment details**:
   - Windows version
   - WiX Toolset version
   - .NET SDK version
   - Visual Studio version (if applicable)

2. **Test scenario**:
   - JSON file structure
   - WiX configuration
   - Installer properties used

3. **Expected vs Actual**:
   - What you expected to happen
   - What actually happened

4. **Log excerpts**:
   - Relevant portions of the MSI log file
   - Any error messages

5. **Reproducibility**:
   - Steps to reproduce the issue
   - Minimal example if possible

## Additional Resources

- [WiX Documentation](https://wixtoolset.org/docs/)
- [JSONPath Online Evaluator](https://jsonpath.com/)
- [MSI Log Analysis](https://docs.microsoft.com/en-us/windows/win32/msi/windows-installer-logging)
