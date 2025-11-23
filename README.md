# WixJsonFileExtension

[![NuGet](https://img.shields.io/nuget/v/WixJsonFileExtension.svg)](https://www.nuget.org/packages/WixJsonFileExtension/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

An extension to [Windows Installer XML (WiX) Toolset](http://wixtoolset.org/) to create or modify JSON-formatted files during an installation.

## Table of Contents

- [Overview](#overview)
- [Status](#status)
- [Prerequisites](#prerequisites)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Detailed Usage](#detailed-usage)
  - [Available Actions](#available-actions)
  - [JsonFile Element Attributes](#jsonfile-element-attributes)
  - [JSONPath vs JSONPointer](#jsonpath-vs-jsonpointer)
- [Examples](#examples)
  - [Reading Values](#reading-values)
  - [Setting Values](#setting-values)
  - [Replacing JSON Objects](#replacing-json-objects)
  - [Deleting Values](#deleting-values)
  - [Creating New Values with JSONPointer](#creating-new-values-with-jsonpointer)
  - [Complex Nested Paths](#complex-nested-paths)
- [Advanced Features](#advanced-features)
  - [Automatic Rollback Support](#automatic-rollback-support)
  - [Scheduling and Service Dependencies](#scheduling-and-service-dependencies)
  - [Creating New JSON Files](#creating-new-json-files)
- [Escaping Special Characters](#escaping-special-characters)
- [Troubleshooting](#troubleshooting)
- [Building from Source](#building-from-source)
- [Contributing](#contributing)
- [Acknowledgements](#acknowledgements)
- [License](#license)

## Overview

[Windows Installer XML (WiX)](http://wixtoolset.org/) is an open-source set of tools to create Windows software installation setups (\*.msi), using XML files to define the content and behavior of the setup.

One important step in most setups is to modify configuration files to reflect either settings specified during the setup by the user, or other settings specific to the individual installation. Windows Installer has built-in actions to modify classic ini-files (\*.ini), and WiX provides extensions to modify XML files, particularly Application Configuration files (\*.exe.config or \*.dll.config) used by Microsoft .NET applications.

Today, a third format has become popular, especially in web applications: [JSON](https://www.json.org/) (JavaScript Object Notation). JSON is commonly used in networking scenarios (REST services, web applications) and as a local data store format, including configuration files. [.NET Core](https://github.com/dotnet/core) and modern .NET applications rely heavily on JSON-formatted configuration files (e.g., `appsettings.json`).

**WixJsonFileExtension** fills this gap by providing methods to read, create, modify, and delete values in JSON files (\*.json) during software installation. The XML elements provided by this extension work similarly to WiX's existing [XmlFile](http://wixtoolset.org/documentation/manual/v3/xsd/util/xmlfile.html) extension.

## Status

✅ **JSONPath supported for install-only applications**

The extension currently supports JSONPath syntax for querying and modifying JSON files during installation. More complex scenarios with multi-select queries inside arrays may have limited support. Please report issues for specific use cases that don't work as expected.

## Prerequisites

- **WiX Toolset**: Version 4.x or later (for WiX v5/v6 support)
- **.NET**: .NET Standard 2.0 compatible environment for building installers
- **Windows**: Windows operating system for running the MSI installers

## Installation

Add the WixJsonFileExtension package to your WiX installer project:

### Using .NET CLI

```bash
dotnet add package WixJsonFileExtension
```

### Using Package Manager Console

```powershell
Install-Package WixJsonFileExtension
```

### Using PackageReference in .csproj/.wixproj

```xml
<PackageReference Include="WixJsonFileExtension" Version="6.0.0" />
```

## Quick Start

1. **Add the namespace** to your WiX source file (.wxs):

```xml
<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs"
     xmlns:Json="http://schemas.hegsie.com/wix/JsonExtension">
```

2. **Use the JsonFile element** inside a Component to modify your JSON file:

```xml
<Component Id="ProductComponent" Guid="{YOUR-GUID-HERE}">
  <File Id="JsonConfig" Name="appsettings.json" Source="appsettings.json" />
  
  <!-- Set a simple value -->
  <Json:JsonFile 
    Id="SetConnectionString" 
    File="[#JsonConfig]" 
    ElementPath="$.ConnectionStrings.Default" 
    Value="Server=myserver;Database=mydb" 
    Action="setValue" />
</Component>
```

3. **Build your installer** as usual with WiX.

## Detailed Usage

### Available Actions

The `JsonFile` element supports the following actions:

| Action | Description | ElementPath Type | Use Case |
|--------|-------------|------------------|----------|
| `readValue` | Reads a value from the JSON file and stores it in a Windows Installer property | JSONPath | Reading configuration values to use elsewhere in the installer |
| `setValue` | Sets or updates a value at the specified path (default action if not specified) | JSONPath | Updating existing JSON properties |
| `deleteValue` | Deletes the value(s) at the specified path | JSONPath | Removing configuration entries |
| `replaceJsonValue` | Replaces an entire JSON object or array with new JSON content | JSONPath | Replacing complex nested structures |
| `createJsonPointerValue` | Creates a new value using JSONPointer syntax (useful for creating nested paths that don't exist) | JSONPointer | Creating new configuration sections |

### JsonFile Element Attributes

| Attribute | Required | Description |
|-----------|----------|-------------|
| `Id` | Yes | Unique identifier for this JSON file modification |
| `File` | Yes | Path to the JSON file to modify. Can use file references like `[#FileId]` or formatted paths like `[INSTALLFOLDER]config.json` |
| `ElementPath` | Yes | JSONPath or JSONPointer expression to locate the element(s) to modify |
| `Action` | No | The action to perform (see Available Actions table). Defaults to `setValue` |
| `Value` | Conditional | The value to set. Required for `setValue`, `replaceJsonValue`, and `createJsonPointerValue` actions. Can be a simple value, property reference like `[PROPERTY_NAME]`, or JSON-formatted string |
| `DefaultValue` | No | Default value to use if the path doesn't exist (used with `readValue`) |
| `Property` | Conditional | Windows Installer property to store the read value. Required for `readValue` action |
| `Sequence` | No | Order in which modifications are applied (default: 1). Lower numbers execute first. Use this to ensure JSON changes happen before services start or in a specific order. All JSON modifications run after `InstallFiles` and before `StartServices` in the standard InstallExecuteSequence |

### JSONPath vs JSONPointer

This extension supports two syntaxes for navigating JSON structures:

#### JSONPath (Default)

JSONPath is the default syntax and is used for most actions. It's more powerful for querying and supports wildcards and filters.

**Syntax examples:**
- `$.store.book` - Access the 'book' property under 'store'
- `$.store.book[0]` - First book in the array
- `$.store.book[?(@.isbn == '0-553-21311-3')]` - Filter books by ISBN
- `$..price` - All 'price' properties at any depth

**Special note**: Square brackets in JSONPath must be escaped in WiX XML as `[\[]` and `[\]]` because they are MSI formatting characters.

#### JSONPointer

JSONPointer is used specifically with the `createJsonPointerValue` action. It's simpler and better suited for creating new paths.

**Syntax examples:**
- `/store/book` - Access the 'book' property under 'store'
- `/store/book/0` - First book in the array
- `/NonExisting/Path` - Creates nested path that doesn't exist

## Examples

All examples assume you have a JSON file installed as part of your component:

```xml
<Component Id="ProductComponent" Guid="{YOUR-GUID-HERE}">
  <File Id="JsonConfig" Name="appsettings.json" Source="appsettings.json" />
  <!-- JsonFile modifications here -->
</Component>
```

### Reading Values

Read a value from JSON and store it in a Windows Installer property:

```xml
<!-- Read a category from a book with specific ISBN -->
<Json:JsonFile 
  Id="ReadBookCategory" 
  File="[#JsonConfig]" 
  ElementPath="$.store.book[\[]?(@.isbn == '0-553-21311-3')[\]].category" 
  DefaultValue="Unknown" 
  Action="readValue" 
  Property="BOOK_CATEGORY" />

<!-- Read a missing value with default -->
<Json:JsonFile 
  Id="ReadMissingValue" 
  File="[#JsonConfig]" 
  ElementPath="$.nonexistent.path" 
  DefaultValue="DefaultValue" 
  Action="readValue" 
  Property="MY_PROPERTY" />
```

### Setting Values

Update or set a simple value:

```xml
<!-- Set a simple string value -->
<Json:JsonFile 
  Id="SetLogLevel" 
  File="[#JsonConfig]" 
  ElementPath="$.Logging.LogLevel.Default" 
  Value="Information" 
  Action="setValue" />

<!-- Set a numeric value -->
<Json:JsonFile 
  Id="SetPrice" 
  File="[#JsonConfig]" 
  ElementPath="$.store.book[\[]?(@.isbn == '0-553-21311-3')[\]].price" 
  Value="12.99" 
  Action="setValue" />

<!-- Set using a Windows Installer property -->
<Json:JsonFile 
  Id="SetFromProperty" 
  File="[#JsonConfig]" 
  ElementPath="$.store.book[\[]?(@.isbn == '0-553-21311-3')[\]].category" 
  Value="[USER_SELECTED_CATEGORY]" 
  Action="setValue" />
```

### Replacing JSON Objects

Replace an entire JSON object or array with new content:

```xml
<!-- Replace an entire array with JSON from a property -->
<Json:JsonFile 
  Id="ReplaceBooks" 
  File="[#JsonConfig]" 
  ElementPath="$.store.book" 
  Value="[MY_BOOKS_JSON]" 
  Action="replaceJsonValue" />
```

The `MY_BOOKS_JSON` property would contain valid JSON like:
```json
[{"title": "New Book", "author": "John Doe", "price": 15.99}]
```

### Deleting Values

Remove values from the JSON file:

```xml
<!-- Delete a specific property -->
<Json:JsonFile 
  Id="DeleteOldPrice" 
  File="[#JsonConfig]" 
  ElementPath="$.store.bicycle.OldPrice" 
  Action="deleteValue" />

<!-- Delete all 'OldPrice' properties at any depth -->
<Json:JsonFile 
  Id="DeleteAllOldPrices" 
  File="[#JsonConfig]" 
  ElementPath="$..book[\[]*[\]].OldPrice" 
  Action="deleteValue" />
```

### Creating New Values with JSONPointer

Create new configuration paths that don't exist:

```xml
<!-- Create a new nested property -->
<Json:JsonFile 
  Id="CreateNewSetting" 
  File="[#JsonConfig]" 
  ElementPath="/NewSection/MySetting" 
  Value="MyValue" 
  Action="createJsonPointerValue" />
```

This will create the structure:
```json
{
  "NewSection": {
    "MySetting": "MyValue"
  }
}
```

### Complex Nested Paths

Working with deeply nested JSON structures:

```xml
<!-- Update a deeply nested configuration value -->
<Json:JsonFile 
  Id="UpdateLogPath" 
  File="[#JsonConfig]" 
  ElementPath="$.Serilog.WriteTo[\[]0[\]].Args.configure[\[]0[\]].Args.path" 
  Value="[LOG_DIRECTORY]MyApp\\Logs\\app.log" />
```

For the JSON structure:
```json
{
  "Serilog": {
    "WriteTo": [
      {
        "Args": {
          "configure": [
            {
              "Args": {
                "path": "C:\\Logs\\app.log"
              }
            }
          ]
        }
      }
    ]
  }
}
```

## Advanced Features

### Automatic Rollback Support

WixJsonFileExtension automatically provides rollback support for all JSON file modifications. If an installation fails or is rolled back, all JSON files are restored to their original state.

**How it works:**
- Before modifying any JSON file, the extension saves a backup of the original file content
- If the installation fails or is cancelled, the rollback custom action automatically restores all modified files
- The file's last modified timestamp is also preserved during rollback
- Rollback happens automatically - no additional configuration is required

**Example:**
```xml
<Json:JsonFile 
  Id="SetConnectionString" 
  File="[#JsonConfig]" 
  ElementPath="$.ConnectionStrings.Default" 
  Value="Server=myserver;Database=mydb" 
  Action="setValue" />
```

If the installation fails after this modification, the `appsettings.json` file will be automatically restored to its original state.

### Scheduling and Service Dependencies

To ensure JSON configuration changes are applied before Windows services start or applications launch, use the `Sequence` attribute to control the order of operations.

**Best Practices:**
1. Use lower sequence numbers (e.g., 1-10) for critical configuration that must be set first
2. Use higher sequence numbers (e.g., 100+) for optional configuration
3. JSON modifications run after `InstallFiles` by default, which is before `StartServices`

**Example - Configure before service starts:**
```xml
<Component Id="AppComponent" Guid="{YOUR-GUID}">
  <File Id="AppConfig" Name="appsettings.json" Source="appsettings.json" />
  
  <!-- These run in order before the service starts -->
  <Json:JsonFile 
    Id="SetDatabaseConnection" 
    File="[#AppConfig]" 
    ElementPath="$.Database.ConnectionString" 
    Value="[DATABASE_CONNECTION]" 
    Sequence="1" />
    
  <Json:JsonFile 
    Id="SetLogging" 
    File="[#AppConfig]" 
    ElementPath="$.Logging.Level" 
    Value="Information" 
    Sequence="2" />
  
  <!-- Service definition comes after JSON modifications -->
  <ServiceInstall 
    Id="MyAppService"
    Name="MyApp"
    Type="ownProcess"
    Start="auto"
    ErrorControl="normal"
    Description="My Application Service" />
    
  <ServiceControl 
    Id="StartService"
    Name="MyApp"
    Start="install"
    Stop="both"
    Remove="uninstall" />
</Component>
```

**Execution Order:**
1. Files are installed (including `appsettings.json`)
2. JSON modifications run (in sequence order)
3. Services are started

This ensures your application configuration is ready before the service attempts to read it.

### Creating New JSON Files

While the extension primarily modifies existing JSON files, you can create new configuration files using the `createJsonPointerValue` action. For more complex scenarios, combine file installation with JSON modifications.

**Scenario 1: Create new settings in an existing file**
```xml
<Json:JsonFile 
  Id="CreateNewSettings" 
  File="[#JsonConfig]" 
  ElementPath="/AppSettings/NewFeature/Enabled" 
  Value="true" 
  Action="createJsonPointerValue" />
```

This creates nested structure:
```json
{
  "AppSettings": {
    "NewFeature": {
      "Enabled": true
    }
  }
}
```

**Scenario 2: Start from a template file**

Ship a template JSON file with your installer and modify it during installation:

```xml
<Component Id="ConfigComponent" Guid="{YOUR-GUID}">
  <!-- Install template as the base configuration -->
  <File Id="ConfigTemplate" Name="config.json" Source="config.template.json" />
  
  <!-- Customize with user/install-specific values -->
  <Json:JsonFile 
    Id="SetInstallPath" 
    File="[#ConfigTemplate]" 
    ElementPath="$.Installation.Path" 
    Value="[INSTALLFOLDER]" 
    Sequence="1" />
    
  <Json:JsonFile 
    Id="SetMachineName" 
    File="[#ConfigTemplate]" 
    ElementPath="$.Installation.MachineName" 
    Value="[COMPUTERNAME]" 
    Sequence="2" />
</Component>
```

**Scenario 3: Create minimal JSON from scratch**

Use multiple `createJsonPointerValue` operations to build a configuration file:

```xml
<Component Id="MinimalConfig" Guid="{YOUR-GUID}">
  <!-- Install an empty or minimal JSON file -->
  <File Id="EmptyConfig" Name="settings.json" Source="empty.json" />
  
  <!-- Build the configuration structure -->
  <Json:JsonFile 
    Id="CreateAppName" 
    File="[#EmptyConfig]" 
    ElementPath="/Application/Name" 
    Value="MyApplication" 
    Action="createJsonPointerValue" 
    Sequence="1" />
    
  <Json:JsonFile 
    Id="CreateAppVersion" 
    File="[#EmptyConfig]" 
    ElementPath="/Application/Version" 
    Value="1.0.0" 
    Action="createJsonPointerValue" 
    Sequence="2" />
    
  <Json:JsonFile 
    Id="CreateDbConnection" 
    File="[#EmptyConfig]" 
    ElementPath="/Database/ConnectionString" 
    Value="[CONNECTION_STRING]" 
    Action="createJsonPointerValue" 
    Sequence="3" />
</Component>
```

Where `empty.json` contains:
```json
{}
```

## Escaping Special Characters

When working with JSONPath in WiX XML files, you need to be aware of multiple layers of escaping:

### 1. MSI Formatting Characters

Square brackets `[` and `]` are special characters in Windows Installer formatted strings. They must be escaped as `[\[]` and `[\]]` in your WiX source files.

**Example:**
```xml
<!-- Wrong: Will be interpreted as MSI property -->
ElementPath="$.store.book[0].title"

<!-- Correct: Escaped for MSI -->
ElementPath="$.store.book[\[]0[\]].title"
```

### 2. JSONPath Escaping

If your JSON keys contain special characters or if you need literal backslashes in paths, remember that JSONPath uses backslashes for escaping.

**Example for a JSON key with a dot:**
```xml
ElementPath="$.store['book.special'].title"
```

### 3. File Paths in Values

When setting file paths as values, remember that Windows uses backslashes, and you may need to escape them:

```xml
<!-- Use double backslashes for literal backslashes -->
Value="C:\\Program Files\\MyApp\\config.json"

<!-- Or use Windows Installer directory properties -->
Value="[INSTALLFOLDER]config.json"
```

## Troubleshooting

### Common Issues

1. **"Element not found" errors**
   - Verify your JSONPath is correct using online JSONPath evaluators
   - Check that square brackets are properly escaped: `[\[]` and `[\]]`
   - Ensure the JSON file is valid and well-formed

2. **Value not being updated**
   - Check the `Sequence` attribute if you have multiple modifications
   - Ensure the path exists before trying to set a value (or use `createJsonPointerValue`)
   - Verify the file reference `[#FileId]` matches your File element's Id

3. **Complex queries not working**
   - Some advanced JSONPath features (like multi-select in arrays) may have limited support
   - Try simplifying your query or report the specific case as an issue
   - Consider breaking complex operations into multiple steps

4. **Property values not expanding**
   - Ensure property names are in uppercase and enclosed in brackets: `[MY_PROPERTY]`
   - Check that the property is set before the JsonFile action executes

### Debugging Tips

- Use the `readValue` action to verify paths are correct before modifying
- Test your JSONPath expressions in an online evaluator
- Check the MSI log file for detailed error messages: `msiexec /i YourInstaller.msi /l*v install.log`
- Refer to the example in `TestJsonConfigInstaller/Product.wxs` for working patterns

## Building from Source

### Prerequisites

- Visual Studio 2022 or later
- WiX Toolset v4 or later
- .NET SDK 6.0 or later
- C++ build tools (for native custom action)

### Build Steps

1. Clone the repository:
   ```bash
   git clone https://github.com/hegsie/WixJsonFileExtension.git
   cd WixJsonFileExtension
   ```

2. Restore NuGet packages:
   ```bash
   nuget restore WixJsonFileExtension.sln
   ```

3. Build the solution:
   ```bash
   msbuild /m /p:Configuration=Release WixJsonFileExtension.sln
   ```

4. The built NuGet package will be in:
   ```
   src\wixext\bin\Release\WixJsonFileExtension.{version}.nupkg
   ```

### Project Structure

- `src/ca/` - Native C++ custom action (uses jsoncons for JSON manipulation)
- `src/wixext/` - WiX extension (C# compiler and binder extensions)
- `src/wixlib/` - WiX library with custom action definitions
- `TestJsonConfigInstaller/` - Example WiX installer project demonstrating usage

## Contributing

Contributions are welcome! Please feel free to submit issues, feature requests, or pull requests.

### Reporting Issues

When reporting issues, please include:
- Your WiX Toolset version
- A minimal example demonstrating the problem
- The JSON structure you're working with
- Any relevant error messages from the MSI log

### Testing Your Changes

Before submitting a pull request:
1. Build the solution successfully
2. Test with the example installer in `TestJsonConfigInstaller/`
3. Ensure your changes don't break existing functionality

## Acknowledgements

WixJsonFileExtension uses [jsoncons](https://github.com/danielaparker/jsoncons) by Daniel Parker to read and manipulate JSON files. Special thanks to Daniel Parker for this excellent C++ JSON library.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

**Repository**: https://github.com/hegsie/WixJsonFileExtension  
**NuGet Package**: https://www.nuget.org/packages/WixJsonFileExtension/  
**Author**: Ben Hegarty (hegsie)
