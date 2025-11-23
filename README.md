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

✅ **Full JSONPath Support with Array Operations**

The extension now provides comprehensive JSONPath support powered by jsoncons, including:
- **Multi-select queries** - Query multiple elements at once with wildcards (`$..price`, `$.store.book[*]`)
- **Advanced filters** - Filter arrays with complex conditions (`$.book[?(@.price > 10)]`)
- **Array operations** - Append, insert, and remove array elements
- **Schema validation** - Validate JSON files against JSON schemas to prevent configuration corruption

All operations are performed during installation with full rollback support.

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
| `appendArray` | Appends a value to an array | JSONPath | Adding new items to configuration arrays |
| `insertArray` | Inserts a value at a specific index in an array | JSONPath | Adding items at specific positions in arrays |
| `removeArrayElement` | Removes element(s) from an array by value or path | JSONPath | Removing specific items from configuration arrays |

### JsonFile Element Attributes

| Attribute | Required | Description |
|-----------|----------|-------------|
| `Id` | Yes | Unique identifier for this JSON file modification |
| `File` | Yes | Path to the JSON file to modify. Can use file references like `[#FileId]` or formatted paths like `[INSTALLFOLDER]config.json` |
| `ElementPath` | Yes | JSONPath or JSONPointer expression to locate the element(s) to modify |
| `Action` | No | The action to perform (see Available Actions table). Defaults to `setValue` |
| `Value` | Conditional | The value to set. Required for `setValue`, `replaceJsonValue`, `createJsonPointerValue`, `appendArray`, `insertArray`, and `removeArrayElement` actions. Can be a simple value, property reference like `[PROPERTY_NAME]`, or JSON-formatted string |
| `DefaultValue` | No | Default value to use if the path doesn't exist (used with `readValue`) |
| `Property` | Conditional | Windows Installer property to store the read value. Required for `readValue` action |
| `Sequence` | No | Order in which modifications are applied (default: 1). Lower numbers execute first |
| `Index` | No | For `insertArray` action: specifies the index at which to insert. Use -1 or omit to append to end |
| `SchemaFile` | No | Path to a JSON schema file for validation. The JSON file will be validated against this schema after modifications |

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

### Working with Arrays

#### Appending to Arrays

Add new elements to the end of an array:

```xml
<!-- Append a new book to the books array -->
<Json:JsonFile 
  Id="AppendBook" 
  File="[#JsonConfig]" 
  ElementPath="$.store.book" 
  Value='{"category":"science","author":"Carl Sagan","title":"Cosmos","price":14.99}' 
  Action="appendArray" />
```

#### Inserting into Arrays

Insert elements at specific positions:

```xml
<!-- Insert a book at the beginning of the array (index 0) -->
<Json:JsonFile 
  Id="InsertBook" 
  File="[#JsonConfig]" 
  ElementPath="$.store.book" 
  Value='{"category":"biography","author":"Walter Isaacson","title":"Steve Jobs","price":18.99}' 
  Action="insertArray"
  Index="0" />

<!-- Append to the end using Index="-1" -->
<Json:JsonFile 
  Id="AppendBookWithIndex" 
  File="[#JsonConfig]" 
  ElementPath="$.store.book" 
  Value='{"category":"history","author":"Yuval Noah Harari","title":"Sapiens","price":16.99}' 
  Action="insertArray"
  Index="-1" />
```

#### Removing Array Elements

Remove elements by matching value:

```xml
<!-- Remove all books with a specific ISBN -->
<Json:JsonFile 
  Id="RemoveBook" 
  File="[#JsonConfig]" 
  ElementPath="$.store.book" 
  Value='{"isbn":"0-553-21311-3"}' 
  Action="removeArrayElement" />

<!-- Remove elements using JSONPath filters -->
<Json:JsonFile 
  Id="RemoveExpensiveBooks" 
  File="[#JsonConfig]" 
  ElementPath="$.store.book[\[]?(@.price > 20)[\]]" 
  Action="removeArrayElement" />
```

### JSON Schema Validation

Validate JSON files against a schema to ensure data integrity:

```xml
<!-- Update a value and validate against schema -->
<Json:JsonFile 
  Id="SetServerWithValidation" 
  File="[#JsonConfig]" 
  ElementPath="$.ConnectionStrings.Database" 
  Value="Server=myserver;Database=mydb;Trusted_Connection=True;" 
  Action="setValue"
  SchemaFile="[INSTALLFOLDER]config-schema.json" />
```

Example schema file (`config-schema.json`):

```json
{
  "type": "object",
  "required": ["ConnectionStrings", "Logging"],
  "properties": {
    "ConnectionStrings": {
      "type": "object",
      "properties": {
        "Database": {
          "type": "string"
        }
      }
    },
    "Logging": {
      "type": "object",
      "properties": {
        "LogLevel": {
          "type": "object"
        }
      }
    }
  }
}
```

The installer will fail if the modified JSON does not conform to the schema, preventing configuration corruption.

### Advanced JSONPath Features

The extension supports complex JSONPath expressions powered by jsoncons:

#### Multi-Select Queries

Select multiple elements at once:

```xml
<!-- Update all prices in the store -->
<Json:JsonFile 
  Id="UpdateAllPrices" 
  File="[#JsonConfig]" 
  ElementPath="$..price" 
  Value="9.99" 
  Action="setValue" />
```

#### Array Filters

Use filters to select specific array elements:

```xml
<!-- Update books by a specific author -->
<Json:JsonFile 
  Id="UpdateMelvilleBooks" 
  File="[#JsonConfig]" 
  ElementPath="$.store.book[\[]?(@.author == 'Herman Melville')[\]].price" 
  Value="12.99" 
  Action="setValue" />

<!-- Update all fiction books -->
<Json:JsonFile 
  Id="UpdateFictionBooks" 
  File="[#JsonConfig]" 
  ElementPath="$.store.book[\[]?(@.category == 'fiction')[\]].price" 
  Value="11.99" 
  Action="setValue" />
```

#### Wildcards

Use wildcards to match any element:

```xml
<!-- Access all book elements regardless of position -->
<Json:JsonFile 
  Id="ReadAllBooks" 
  File="[#JsonConfig]" 
  ElementPath="$.store.book[\[]*[\]]" 
  Action="readValue"
  Property="ALL_BOOKS" />
```

#### Complex Filters

Combine multiple conditions:

```xml
<!-- Update expensive fiction books -->
<Json:JsonFile 
  Id="UpdateExpensiveFiction" 
  File="[#JsonConfig]" 
  ElementPath="$.store.book[\[]?(@.category == 'fiction' &amp;&amp; @.price > 10)[\]].price" 
  Value="15.99" 
  Action="setValue" />
```

**Note**: When using logical operators in XML, remember to escape them:
- `&&` becomes `&amp;&amp;`
- `||` becomes `||` (no escaping needed)
- `<` becomes `&lt;`
- `>` becomes `&gt;`

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
   - Verify your JSONPath is correct using online JSONPath evaluators like [JSONPath Online Evaluator](https://jsonpath.com/)
   - Check that square brackets are properly escaped: `[\[]` and `[\]]`
   - Ensure the JSON file is valid and well-formed

2. **Value not being updated**
   - Check the `Sequence` attribute if you have multiple modifications
   - Ensure the path exists before trying to set a value (or use `createJsonPointerValue`)
   - Verify the file reference `[#FileId]` matches your File element's Id

3. **Array operations failing**
   - Ensure the ElementPath points to an array when using `appendArray`, `insertArray`, or `removeArrayElement`
   - For `insertArray`, verify the Index is valid or use -1 to append
   - For `removeArrayElement`, ensure the Value matches the element structure you want to remove

4. **Schema validation errors**
   - Verify the schema file path is correct and the file is accessible during installation
   - Check that the schema file is valid JSON Schema format
   - Review the MSI log for specific validation error messages
   - Note: This extension provides basic schema validation (type checking, required properties). For full JSON Schema Draft 7+ support, ensure your schemas use supported features

5. **Property values not expanding**
   - Ensure property names are in uppercase and enclosed in brackets: `[MY_PROPERTY]`
   - Check that the property is set before the JsonFile action executes

### Debugging Tips

- Use the `readValue` action to verify paths are correct before modifying
- Test your JSONPath expressions in an online evaluator like [jsonpath.com](https://jsonpath.com/)
- For complex queries, test with the jsoncons library directly or use its online playground
- Check the MSI log file for detailed error messages: `msiexec /i YourInstaller.msi /l*v install.log`
- Refer to the example in `TestJsonConfigInstaller/Product.wxs` for working patterns
- When using schema validation, test your JSON against the schema separately first using tools like [jsonschemavalidator.net](https://www.jsonschemavalidator.net/)

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
