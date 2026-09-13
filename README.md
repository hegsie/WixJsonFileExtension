# WixJsonFileExtension

[![NuGet](https://img.shields.io/nuget/v/WixJsonFileExtension.svg)](https://www.nuget.org/packages/WixJsonFileExtension/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

An extension to [Windows Installer XML (WiX) Toolset](http://wixtoolset.org/) to create or modify JSON-formatted files during an installation.

## Table of Contents

- [Overview](#overview)
- [Status](#status)
- [Compatibility](#compatibility)
  - [WiX Toolset Version Support](#wix-toolset-version-support)
  - [.NET Application Compatibility](#net-application-compatibility)
- [Prerequisites](#prerequisites)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Documentation & Resources](#documentation--resources)
  - [Cookbook - Common Patterns](#cookbook---common-patterns)
  - [Example Fragments](#example-fragments)
- [Improved Authoring Experience](#improved-authoring-experience)
  - [XSD Schema with IntelliSense](#xsd-schema-with-intellisense)
  - [Composite Elements for Common Patterns](#composite-elements-for-common-patterns)
  - [Grouping with JsonTransaction](#grouping-with-jsontransaction)
- [Detailed Usage](#detailed-usage)
  - [Available Actions](#available-actions)
  - [JsonFile Element Attributes](#jsonfile-element-attributes)
  - [Value Typing](#value-typing)
  - [Property Expansion and Literal Values](#property-expansion-and-literal-values)
  - [File Writes](#file-writes)
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
  - [Install and Uninstall Timing](#install-and-uninstall-timing)
  - [Backups and Restore on Uninstall](#backups-and-restore-on-uninstall)
  - [Creating New JSON Files](#creating-new-json-files)
- [Common .NET Configuration Patterns](#common-net-configuration-patterns)
  - [Connection Strings](#connection-strings)
  - [Logging Configuration](#logging-configuration)
  - [Serilog Configuration](#serilog-configuration)
  - [ASP.NET Core Application Settings](#aspnet-core-application-settings)
  - [Environment-Specific Configuration](#environment-specific-configuration)
  - [Service Configuration](#service-configuration)
  - [API Keys and Secrets](#api-keys-and-secrets)
  - [Complete Example: Typical .NET 6+ Application](#complete-example-typical-net-6-application)
  - [Best Practices for .NET Configuration](#best-practices-for-net-configuration)
- [Escaping Special Characters](#escaping-special-characters)
- [Diagnostics](#diagnostics)
  - [Dry Run](#dry-run)
  - [Verbose Logging](#verbose-logging)
  - [Transform Log](#transform-log)
  - [Command-Line Harness (jsoncli)](#command-line-harness-jsoncli)
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


## Compatibility

### WiX Toolset Version Support

This extension supports **WiX Toolset v4, v5, v6 and v7**. Every flavour targets .NET Standard 2.0 and shares the same sources and the same wixlib; only the toolset it is compiled against differs.

| WiX Version | Support Status | Notes |
|-------------|----------------|-------|
| **WiX 7.x** | ✅ Fully Supported | Built against WiX 7.0.0; shipped in `wixext7/` |
| **WiX 6.x** | ✅ Fully Supported | Built against WiX 6.0.1; shipped in `wixext6/` |
| **WiX 5.x** | ✅ Fully Supported | Built against WiX 5.0.2; shipped in `wixext5/` |
| **WiX 4.x** | ✅ Fully Supported | Built against WiX 4.0.6; shipped in `wixext4/` |
| WiX 3.x | ❌ Not Supported | Use WiX 4+ for this extension |

**Package distribution**: The extension is distributed via NuGet with binaries in the `wixext4/`, `wixext5/`, `wixext6/` and `wixext7/` package folders. WiX picks the folder matching its own major version, so nothing needs to be configured beyond the `PackageReference` - installing the package into a WiX 4, 5, 6 or 7 project just works. Every push builds a package with each of those four toolsets to keep that true.

Extension assemblies cannot be shared across WiX major versions: `WixToolset.Data` and `WixToolset.Extensibility` bump their assembly version with every major release (4.0.0.0, 5.0.0.0, 6.0.0.0, 7.0.0.0) and an extension binds to the version it was compiled against. .NET resolves such a reference forward to a higher assembly version but never back to a lower one, so one assembly cannot serve every toolset. That is why the package carries one assembly per version.

**WiX 7 and the Open Source Maintenance Fee**: WiX v7 packages and tools refuse to run until the [OSMF EULA](https://wixtoolset.org/osmf/) is accepted (error `WIX7015`). This applies to your own build, independently of this extension - accept it once with `wix eula` (or `dotnet build -p:AcceptEula=wix7`) as described in the WiX documentation.

### .NET Application Compatibility

This extension works with all .NET application types that use JSON configuration files:

| Application Type | Configuration Files | Support Status |
|------------------|---------------------|----------------|
| **.NET 6+ / .NET Core** | `appsettings.json`, `appsettings.{Environment}.json` | ✅ Fully Supported |
| **.NET Framework** | `app.config`, `web.config` (XML), custom JSON files | ✅ JSON files supported |
| **ASP.NET Core** | `appsettings.json` | ✅ Fully Supported |
| **Console Apps** | `appsettings.json`, custom JSON configs | ✅ Fully Supported |
| **Windows Services** | `appsettings.json`, custom JSON configs | ✅ Fully Supported |

**Hybrid scenarios**: Applications using both classic `.config` (XML) and modern JSON files can leverage both WiX's built-in XML handling and this extension for JSON files.

### Installer Platform Support

The custom action DLL ships for all Windows Installer platforms, and the extension automatically references the variant matching your package's platform:

| MSI Platform | Support Status |
|--------------|----------------|
| **x64** | ✅ Supported |
| **x86** | ✅ Supported |
| **ARM64** | ✅ Supported |

Note: when building the extension from source, a plain x64 solution build produces an x64-only wixlib; build `src/ca/jsonca.vcxproj` for `Win32` and `ARM64` first (as CI does) to produce the multi-platform wixlib.

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
<PackageReference Include="WixJsonFileExtension" Version="7.0.0" />
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

## Documentation & Resources

### Cookbook - Common Patterns

The **[Cookbook](docs/COOKBOOK.md)** provides practical recipes and patterns for common JSON configuration scenarios:

- 📚 **[Connection Strings](docs/COOKBOOK.md#connection-strings)** - Configure database connections during installation
- 📝 **[Logging Configuration](docs/COOKBOOK.md#logging-configuration)** - Set log levels, file paths, and Serilog settings
- 🚩 **[Feature Flags](docs/COOKBOOK.md#feature-flags)** - Toggle features based on edition or environment
- 🌐 **[API Endpoints](docs/COOKBOOK.md#api-endpoints)** - Configure API URLs, timeouts, and authentication
- 🔧 **[Environment-Specific Settings](docs/COOKBOOK.md#environment-specific-settings)** - Deploy configuration for Development, Staging, or Production
- 📐 **[Complex Nested Configurations](docs/COOKBOOK.md#complex-nested-configurations)** - Work with deeply nested JSON structures
- 🔗 **[Best Practices](docs/COOKBOOK.md#best-practices)** - Tips for effective JSON file manipulation

### Example Fragments

The **[examples/](examples/)** directory contains ready-to-use WiX fragment files:

- **[ConnectionStrings.wxs](examples/ConnectionStrings.wxs)** - Database connection configuration
- **[LoggingConfiguration.wxs](examples/LoggingConfiguration.wxs)** - Logging levels and file paths
- **[FeatureFlags.wxs](examples/FeatureFlags.wxs)** - Feature flag management
- **[ApiEndpoints.wxs](examples/ApiEndpoints.wxs)** - API endpoint configuration
- **[EnvironmentConfiguration.wxs](examples/EnvironmentConfiguration.wxs)** - Environment-based settings
- **[AdvancedArrayOperations.wxs](examples/AdvancedArrayOperations.wxs)** ⭐ NEW - Advanced array operations and conditional updates
- **[CompositeElements.wxs](examples/CompositeElements.wxs)** ⭐ NEW - High-level composite elements and JsonTransaction grouping

See the [Examples README](examples/README.md) for usage instructions.

## Improved Authoring Experience

WixJsonFileExtension v6.0 includes significant improvements to the authoring experience, making it easier to write and maintain WiX installers that manipulate JSON files.

### XSD Schema with IntelliSense

The extension now includes a comprehensive XSD schema file (`json.xsd`) that provides rich IntelliSense support in Visual Studio, VS Code, and other XML editors.

**Features:**
- 📝 **Detailed attribute descriptions** - Hover over any attribute to see usage information
- 🎯 **Strict typing for Action attribute** - Enum values with descriptions for each action type
- ✅ **Validation at design time** - Catch errors before building
- 🔍 **Auto-completion** - All elements and attributes available in IntelliSense
- 📖 **Context-sensitive help** - Understand what each attribute does while authoring

**Using the XSD Schema:**

The schema is automatically available when you reference the extension namespace:

```xml
<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs"
     xmlns:Json="http://schemas.hegsie.com/wix/JsonExtension">
```

**For VS Code users:** Configure the [XML extension](https://marketplace.visualstudio.com/items?itemName=redhat.vscode-xml) to get full IntelliSense support. The schema file is included in the NuGet package under the `schemas/` folder.

### Composite Elements for Common Patterns

Instead of writing verbose `JsonFile` elements for common .NET configuration patterns, you can now use high-level composite elements that expand into the appropriate low-level operations.

#### Json:AppSettings

Simplified syntax for .NET application settings:

```xml
<!-- Traditional approach -->
<Json:JsonFile 
  Id="SetEnvironment" 
  File="[#AppSettings]" 
  ElementPath="$.ApplicationSettings.Environment" 
  Value="Production" 
  Action="setValue" />

<!-- New composite element approach -->
<Json:AppSettings 
  Id="SetEnvironment" 
  File="[#AppSettings]" 
  Key="ApplicationSettings.Environment" 
  Value="Production" />
```

**Attributes:**
- `Key` - Dot-notation path (automatically converted to JSONPath)
- `Value` - The value to set
- `CreateIfMissing` - (Optional) Create nested path if it doesn't exist (default: yes). When `yes`, the setting is written with the `createJsonPointerValue` action so missing keys (including intermediate objects) are created. When `no`, the setting is only updated if the key already exists; missing keys are skipped without failing the install

#### Json:ConnectionString

Simplified syntax for database connection strings:

```xml
<!-- Traditional approach -->
<Json:JsonFile 
  Id="SetConnection" 
  File="[#AppSettings]" 
  ElementPath="$.ConnectionStrings.DefaultConnection" 
  Value="Server=myserver;Database=mydb;" 
  Action="setValue" />

<!-- New composite element approach -->
<Json:ConnectionString 
  Id="SetConnection" 
  File="[#AppSettings]" 
  Name="DefaultConnection" 
  Value="Server=myserver;Database=mydb;" />
```

**Attributes:**
- `Name` - The connection string name (e.g., "DefaultConnection")
- `Value` - The connection string value

**Security Note:** The composite element includes IntelliSense documentation warning about storing passwords in plain text and recommending Integrated Security or external secret management.

#### Json:LoggingLevel

Simplified syntax for .NET logging configuration:

```xml
<!-- Traditional approach -->
<Json:JsonFile 
  Id="SetLogLevel" 
  File="[#AppSettings]" 
  ElementPath="$.Logging.LogLevel.Default" 
  Value="Information" 
  Action="setValue" />

<!-- New composite element approach -->
<Json:LoggingLevel 
  Id="SetLogLevel" 
  File="[#AppSettings]" 
  Category="Default" 
  Level="Information" />
```

**Attributes:**
- `Category` - Logging category (default: "Default", also supports "Microsoft", "System", or custom namespaces)
- `Level` - Log level (Trace, Debug, Information, Warning, Error, Critical, None)

**Benefits of Composite Elements:**
- 🎯 **Cleaner syntax** - Less verbose than low-level JsonFile elements
- 📚 **Self-documenting** - Element names clearly indicate purpose
- ✅ **Type safety** - Specific attributes for each pattern
- 🔒 **Best practices** - Follows .NET conventions automatically
- 📖 **Better IntelliSense** - Contextual help for .NET-specific configurations

### Grouping with JsonTransaction

For complex JSON manipulations that involve multiple related edits, you can now use the `JsonTransaction` element to group operations together for cleaner authoring.

**Example:**

```xml
<Component Id="DatabaseConfig" Guid="{YOUR-GUID}">
  <File Id="AppSettings" Name="appsettings.json" Source="appsettings.json" />
  
  <!-- Group all database-related configuration together -->
  <Json:JsonTransaction 
    Id="DatabaseConfiguration" 
    File="[#AppSettings]" 
    BaseSequence="10">
    
    <!-- All nested JsonFile elements automatically use the same file -->
    <!-- Sequences are automatically assigned: 10, 11, 12, 13 -->
    <Json:JsonFile 
      Id="SetConnection" 
      ElementPath="$.ConnectionStrings.DefaultConnection" 
      Value="Server=[DB_SERVER];Database=[DB_NAME];" 
      Action="setValue" />
    
    <Json:JsonFile 
      Id="SetTimeout" 
      ElementPath="$.Database.CommandTimeout" 
      Value="30" 
      Action="setValue" />
    
    <Json:JsonFile 
      Id="SetRetry" 
      ElementPath="$.Database.MaxRetryCount" 
      Value="3" 
      Action="setValue" />
    
    <Json:JsonFile 
      Id="SetPoolSize" 
      ElementPath="$.Database.MaxPoolSize" 
      Value="100" 
      Action="setValue" />
  </Json:JsonTransaction>
</Component>
```

**Attributes:**
- `Id` - Unique identifier for the transaction group
- `File` - (Optional) Default file path for all nested JsonFile elements
- `BaseSequence` - (Optional) Base sequence number for automatic sequencing (default: 1)

**Benefits:**
- 🗂️ **Logical grouping** - Related operations are visually grouped together
- 🔢 **Automatic sequencing** - No need to manually assign sequence numbers
- 📝 **Reduced repetition** - File path specified once for all operations
- 📚 **Better maintainability** - Easier to understand and modify complex configurations
- 🎯 **Clear intent** - Transaction name documents the purpose of the grouped operations

**Complete Example:**

```xml
<Component Id="AppConfig" Guid="{YOUR-GUID}">
  <File Id="ProdConfig" Name="appsettings.Production.json" Source="appsettings.json" />
  
  <!-- Use composite elements for simple settings -->
  <Json:ConnectionString 
    Id="MainDb" 
    File="[#ProdConfig]" 
    Name="MainDatabase" 
    Value="Server=[DB_SERVER];Database=[DB_NAME];Integrated Security=true;" />
  
  <Json:LoggingLevel 
    Id="DefaultLog" 
    File="[#ProdConfig]" 
    Level="Warning" />
  
  <!-- Use JsonTransaction for complex related settings -->
  <Json:JsonTransaction 
    Id="CacheConfiguration" 
    File="[#ProdConfig]" 
    BaseSequence="10">
    
    <Json:JsonFile 
      Id="EnableCache" 
      ElementPath="$.CacheSettings.EnableCaching" 
      Value="true" 
      Action="setValue" />
    
    <Json:JsonFile 
      Id="CacheExpiry" 
      ElementPath="$.CacheSettings.DefaultExpirationMinutes" 
      Value="60" 
      Action="setValue" />
  </Json:JsonTransaction>
</Component>
```

See **[CompositeElements.wxs](examples/CompositeElements.wxs)** for more comprehensive examples.


## Detailed Usage

### Available Actions

The `JsonFile` element supports the following actions:

| Action | Description | ElementPath Type | Use Case |
|--------|-------------|------------------|----------|
| `readValue` | Reads a value from the JSON file and stores it in a Windows Installer property | JSONPath | Reading configuration values to use elsewhere in the installer |
| `setValue` | Sets or updates a value at the specified path (default action if not specified) | JSONPath | Updating existing JSON properties |
| `deleteValue` | Deletes the value(s) at the specified path | JSONPath | Removing configuration entries |
| `replaceJsonValue` | Replaces an entire JSON object or array with new JSON content | JSONPath | Replacing complex nested structures |
| `createJsonPointerValue` | Sets a value using JSONPointer syntax, creating the path (including intermediate objects) if it doesn't exist. Existing values at the path are overwritten | JSONPointer | Creating new configuration sections |
| `appendArray` | Appends a value to an array | JSONPath | Adding new items to configuration arrays |
| `insertArray` | Inserts a value at a specific index in an array | JSONPath | Adding items at specific positions in arrays |
| `removeArrayElement` | Removes element(s) from an array by value or path | JSONPath | Removing specific items from configuration arrays |
| `distinctValues` | Removes duplicate values from an array | JSONPath | Ensuring unique values in configuration arrays |

### JsonFile Element Attributes

| Attribute | Required | Description |
|-----------|----------|-------------|
| `Id` | Yes | Unique identifier for this JSON file modification |
| `File` | Yes | Path to the JSON file to modify. Can use file references like `[#FileId]` or formatted paths like `[INSTALLFOLDER]config.json` |
| `ElementPath` | Yes | JSONPath or JSONPointer expression to locate the element(s) to modify |
| `Action` | No | The action to perform (see Available Actions table). Defaults to `setValue` |
| `Value` | Conditional | The value to set. Required for `setValue`, `replaceJsonValue`, `createJsonPointerValue`, `appendArray`, and `insertArray` actions. For `removeArrayElement`, `Value` is optional: if omitted, elements matched by the JSONPath expression are removed; if provided, all array elements matching that value are removed. Can be a simple value, property reference like `[PROPERTY_NAME]`, or JSON-formatted string. See "Value typing" below for how values are converted to JSON types |
| `ValueType` | No | How `Value` is converted to JSON: `auto` (default), `string`, `number`, `boolean`, `null`, `json` or `date`. See [Value Typing](#value-typing) |
| `Culture` | No | Culture name (e.g. `de-DE`, `en-GB`) whose conventions `ValueType="number"` and `ValueType="date"` are parsed with. Without it the invariant culture applies |
| `Formatted` | No | Whether `Value` is a Windows Installer formatted string (`[PROPERTY]` and `[%ENV]` expansion, `[\[]` escapes). `no` takes it literally, which is how to author a value containing square brackets such as a JSON array. Default is `yes` |
| `DefaultValue` | No | Default value to use if the value cannot be read - the path doesn't exist, the file doesn't exist, or the file cannot be parsed (used with `readValue`) |
| `Property` | Conditional | Windows Installer property to store the read value. Required for `readValue` action |
| `On` | No | When the modification runs: `install` (default) while the component is being installed or repaired, `uninstall` while it is being uninstalled, or `both`. The same `Action` and `Value` apply at each time; see [Install and Uninstall Timing](#install-and-uninstall-timing) |
| `Sequence` | No | Order in which modifications are applied. Lower numbers execute first. When omitted, elements are automatically assigned increasing sequence numbers in authoring order, so operations on the same file execute deterministically in the order they appear in your source. Use this to ensure JSON changes happen before services start or in a specific order. All JSON modifications run after `InstallFiles` and before `StartServices` in the standard InstallExecuteSequence |
| `Index` | No | For `insertArray` action: specifies the index at which to insert. Use -1 or omit to append to end |
| `SchemaFile` | No | Path to a JSON schema file for validation. The JSON file will be validated against this schema after modifications |
| `CreateBackup` | No | When `yes`, the file is copied to `<File><BackupSuffix>` before the first modification the extension makes to it in a transaction, unless that backup already exists. See [Backups and Restore on Uninstall](#backups-and-restore-on-uninstall). Default is `no` |
| `BackupSuffix` | No | Suffix of the backup file, default `.wixbak`. Requires `CreateBackup="yes"` |
| `RestoreOnUninstall` | No | When `yes`, uninstalling the component copies the backup back over the file and removes the backup, before any `On="uninstall"` modifications of that file. Requires `CreateBackup="yes"`. Default is `no` |
| `OnlyIfExists` | No | When set to `yes`, the action is only performed if the ElementPath already exists in the JSON file. If the path (or the file itself) does not exist, the operation is skipped and the install continues. This is useful for conditional updates that should only modify existing values without creating new ones. Applies to all write actions. Default is `no` |

### Value Typing

How the `Value` attribute is converted into a JSON value is decided by `ValueType`. The default, `auto`, infers the type from the text:

- **Replacing an existing string value** (`setValue`, `createJsonPointerValue`): the new value is always written as a string, so values like `"1.0"` or `"true"` don't silently change type.
- **Replacing a non-string value or creating a new value**: the value is parsed as JSON first, so `9090` becomes a number, `true`/`false` become booleans, and `["a","b"]` becomes an array. If the value is not valid JSON it is written as a string.
- **`appendArray` / `insertArray` / `removeArrayElement`**: the value is parsed as JSON with a fallback to string.
- **`replaceJsonValue`**: the value must be valid JSON and is written exactly as parsed.

When the inference is not what you want, or the value comes from user input in a local format, set the type explicitly. Explicit types convert strictly and **fail the operation** (and so the install) when the text does not fit, which is usually preferable to a silently mistyped setting:

| `ValueType` | Writes | Accepts |
|-------------|--------|---------|
| `string` | a string | anything, including `"0042"` or `"true"`, kept as text |
| `number` | a number | digits with the decimal and grouping separators of `Culture` (`1.234,5` in `de-DE`, `1,234.5` in `en-US`); integers stay integers |
| `boolean` | `true`/`false` | `true`/`false`, `yes`/`no`, `on`/`off`, `1`/`0`, case-insensitive |
| `null` | `null` | anything (the text is ignored) |
| `json` | the parsed JSON | valid JSON only |
| `date` | an ISO 8601 string, `yyyy-MM-ddTHH:mm:ss` | a date (and optional time) in `Culture`'s conventions: `31/12/2024` in `en-GB`, `12/31/2024` in `en-US`, `2024-12-31` anywhere |

Without `Culture`, `number` and `date` use the invariant culture. `Culture` is a formatted field, so it can be set from a property collected in the UI. A well-formed culture name Windows does not know (say `xx-ZZ`) is not an error: Windows treats it as a custom locale with default conventions, so check the name rather than relying on a failure.

```xml
<!-- A port typed by the user stays a number even though the property is text -->
<Json:JsonFile Id="SetPort" File="[#AppConfig]" ElementPath="$.Kestrel.Port" Value="[PORT]" ValueType="number" />

<!-- A German-format threshold from a property -->
<Json:JsonFile Id="SetThreshold" File="[#AppConfig]" ElementPath="$.Limits.Threshold" Value="[THRESHOLD]" ValueType="number" Culture="de-DE" />

<!-- Feature flag from a checkbox property that holds "1" or "" -->
<Json:JsonFile Id="SetFlag" File="[#AppConfig]" ElementPath="$.Features.Beta" Value="[BETA_CHECKED]" ValueType="boolean" />
```

### Property Expansion and Literal Values

`Value`, `File`, `ElementPath`, `SchemaFile`, `BackupSuffix` and `Culture` are Windows Installer **formatted** fields: `[PROPERTY]` is replaced by the property's value, `[%NAME]` by the environment variable, `[#FileId]` and `[$ComponentId]` by installed paths, and a literal `[` or `]` has to be written `[\[]` and `[\]]`. The rules are Windows Installer's own, documented under [Formatted](https://learn.microsoft.com/windows/win32/msi/formatted); unknown property references expand to nothing rather than failing.

Because of that, a `Value` that is itself JSON with square brackets, such as an array, is mangled by formatting unless the brackets are escaped. Rather than escaping, set `Formatted="no"` to have the value used exactly as written, with no expansion of any kind:

```xml
<Json:JsonFile Id="SetOrigins" File="[#AppConfig]" ElementPath="$.Cors.AllowedOrigins"
               Value='["https://a.example","https://b.example"]' Formatted="no" ValueType="json" />
```

A formatted value cannot contain a property reference *and* literal brackets in a convenient way; for those, put the JSON in a property and reference it (`Value="[MY_JSON]"`), as the test installer does.

### File Writes

All modifications are written atomically: the updated JSON is written to a temporary file next to the target and then swapped in, so a failure mid-write can never leave a truncated or corrupted configuration file.

Files are re-serialized on every write, but in a way that keeps diffs small for source-controlled configuration: **object keys keep their order**, and the **indentation unit** (tabs, or two or four spaces), **line endings** (CRLF or LF) and **trailing newline** of the existing file are detected and reused. A file with nothing to detect (minified) is written with four-space indentation and CRLF. Non-standard content such as comments is not preserved (files containing comments fail to parse), and jsoncons' own line-breaking rules apply within a line (short arrays stay on one line).

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

On uninstall the order is mirrored: services are stopped, uninstall-time JSON modifications (`On="uninstall"`) run, and only then are files removed.

### Install and Uninstall Timing

By default a `JsonFile` element runs while its component is being **installed or repaired**. The `On` attribute selects the other moments in a product's life:

| `On` | Runs when | Scheduled |
|------|-----------|-----------|
| `install` (default) | The component is being installed or repaired | After `InstallFiles` |
| `uninstall` | The component is being uninstalled | Before `RemoveFiles`, so the target file still exists |
| `both` | Both of the above | At both points |

The element's `Action` and `Value` are applied exactly as authored at each of those times. There is no separate "uninstall value": like WiX's own `util:XmlConfig`, an uninstall-time change is its own element, usually the revert of an install-time one. Nothing is reverted implicitly. Without `On="uninstall"` elements, JSON changes made at install time are left in place when the product is removed.

**Example - add a plugin entry to a shared configuration file and remove it again on uninstall:**
```xml
<Component Id="PluginRegistration" Guid="{YOUR-GUID}">
  <RegistryValue Root="HKLM" Key="Software\MyApp\Plugin" Name="Registered" Type="integer" Value="1" KeyPath="yes" />

  <!-- Install: register the plugin (creating the section if needed) -->
  <Json:JsonFile Id="RegisterPlugin"
                 File="[CommonAppDataFolder]HostApp\plugins.json"
                 ElementPath="/Plugins/MyPlugin/Path"
                 Value="[INSTALLFOLDER]MyPlugin.dll"
                 Action="createJsonPointerValue" />

  <!-- Uninstall: unregister it. deleteValue with OnlyIfExists tolerates a hand-edited file. -->
  <Json:JsonFile Id="UnregisterPlugin"
                 File="[CommonAppDataFolder]HostApp\plugins.json"
                 ElementPath="$.Plugins.MyPlugin"
                 Action="deleteValue"
                 OnlyIfExists="yes"
                 On="uninstall" />

  <!-- Both: keep a last-touched stamp current whichever way the component goes -->
  <Json:JsonFile Id="StampPlugins"
                 File="[CommonAppDataFolder]HostApp\plugins.json"
                 ElementPath="/Plugins/LastModifiedBy"
                 Value="[ProductName] [ProductVersion]"
                 Action="createJsonPointerValue"
                 On="both" />
</Component>
```

**What to expect across the product lifecycle:**
- **Install:** `On="install"` and `On="both"` elements run. `On="uninstall"` elements are ignored.
- **Repair** (`msiexec /f`): the same as install. A repair is not a removal, so `On="uninstall"` elements are ignored.
- **Uninstall:** `On="uninstall"` and `On="both"` elements run, before `RemoveFiles`. `readValue` with `On="uninstall"` runs in the uninstall's immediate phase, so a property read this way can feed the `Value` of an uninstall-time write.
- **Major upgrade** (with WiX's default `afterInstallValidate` scheduling): the old package's uninstall-time elements run first, then the new package's install-time elements. Property values passed on the upgrade's command line are not visible to the old package's uninstall; it uses its own defaults.
- **Rollback:** uninstall-time changes are captured and restored like install-time ones if the uninstall fails.

Uninstall-time changes are only meaningful for a JSON file that **outlives the component**: a machine-wide or shared configuration file, a file owned by another product, or a file marked permanent. A file installed by the same component is edited and then deleted moments later by `RemoveFiles`, which is harmless but pointless.

### Backups and Restore on Uninstall

`CreateBackup="yes"` on any `JsonFile` element makes the extension copy that element's file to `<File><BackupSuffix>` (default `.wixbak`) before the first modification it makes to the file in a transaction. The backup is taken once: if it already exists it is left alone, so it always holds the file as it was **before the product first touched it**, and a repair, which re-applies the same modifications, does not overwrite it. An administrator can put the original back by copying the backup over the file.

```xml
<Json:JsonFile Id="SetConnection" File="[#AppConfig]" ElementPath="$.ConnectionStrings.Default"
               Value="[DATABASE_CONNECTION]" CreateBackup="yes" BackupSuffix=".orig" />
```

`RestoreOnUninstall="yes"` turns that into a backup-and-restore mode for files that outlive the component: when the component is uninstalled, the backup is copied back over the file and then removed, before `RemoveFiles` and before any `On="uninstall"` modifications of that file run. The file therefore ends up exactly as it was before the product first modified it, whatever the product changed since, and a later reinstall starts a fresh backup.

```xml
<!-- Register with a host application's shared settings file; hand it back untouched on uninstall -->
<Json:JsonFile Id="RegisterPlugin" File="[CommonAppDataFolder]HostApp\settings.json"
               ElementPath="/plugins/MyApp" Value="[INSTALLFOLDER]MyApp.dll"
               Action="createJsonPointerValue" CreateBackup="yes" RestoreOnUninstall="yes" />
```

**Details:**
- One element with `CreateBackup="yes"` is enough for its file: the backup is taken before the first modification of that file in the transaction, whichever element makes it.
- On a major upgrade the old package restores its backup (if `RestoreOnUninstall`) before the new package takes a fresh one.
- Nothing is backed up or restored during a [dry run](#dry-run), and a missing file or missing backup is not an error.
- Backups are not undone by rollback: a failed install leaves the (correct) backup in place next to the restored file.
- The [transform log](#transform-log) records the backup path on the entry that created it, and a `restoreBackup` entry for each restore.

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

#### Removing Duplicates from Arrays

Remove duplicate values from an array to ensure uniqueness:

```xml
<!-- Remove duplicate entries from a tags array -->
<Json:JsonFile 
  Id="DeduplicateTags" 
  File="[#JsonConfig]" 
  ElementPath="$.configuration.tags" 
  Action="distinctValues" />

<!-- Remove duplicates from feature flags array -->
<Json:JsonFile 
  Id="DeduplicateFeatureFlags" 
  File="[#JsonConfig]" 
  ElementPath="$.features.enabled" 
  Action="distinctValues" />
```

The `distinctValues` action compares array elements by their serialized JSON representation, so it works with both simple values (strings, numbers) and complex objects. For example:

Before:
```json
{
  "tags": ["production", "webapp", "production", "backend", "webapp"]
}
```

After `distinctValues`:
```json
{
  "tags": ["production", "webapp", "backend"]
}
```

### Conditional Updates with OnlyIfExists

The `OnlyIfExists` attribute allows you to conditionally update JSON values only if they already exist. This is useful when you want to modify existing configuration without creating new entries.

```xml
<!-- Only update connection string if it already exists -->
<Json:JsonFile 
  Id="UpdateConnectionString" 
  File="[#JsonConfig]" 
  ElementPath="$.ConnectionStrings.Database" 
  Value="Server=myserver;Database=mydb;" 
  Action="setValue"
  OnlyIfExists="yes" />

<!-- Only update log level if logging section exists -->
<Json:JsonFile 
  Id="UpdateLogLevel" 
  File="[#JsonConfig]" 
  ElementPath="$.Logging.LogLevel.Default" 
  Value="Warning" 
  Action="setValue"
  OnlyIfExists="yes" />

<!-- This will NOT create the path if it doesn't exist -->
<Json:JsonFile 
  Id="ConditionalFeature" 
  File="[#JsonConfig]" 
  ElementPath="/Features/NewFeature/Enabled" 
  Value="true" 
  Action="createJsonPointerValue"
  OnlyIfExists="yes" />
```

**Use Cases for OnlyIfExists:**
- Updating optional configuration sections that may not be present in all deployments
- Modifying settings only in environments where they're already configured
- Avoiding creation of unnecessary configuration entries
- Safe upgrades that only modify existing settings

**Behavior:**
- If `OnlyIfExists="yes"` and the path exists: the operation proceeds normally
- If `OnlyIfExists="yes"` and the path does NOT exist: the operation is skipped (returns success, no error)
- If `OnlyIfExists="yes"` and the JSON file itself does NOT exist: the operation is skipped (returns success, no error)
- If `OnlyIfExists="no"` (or omitted): the operation always proceeds (default behavior)

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

The installer will fail if the modified JSON does not conform to the schema, preventing configuration corruption. Every violation is written to the MSI log with its location in the document and the keyword it breaks, for example:

```
Schema violation at '/store/book/0' (required): Required property 'author' not found.
JSON schema validation failed with 1 violation(s)
```

**Schema Validation Capabilities:**

Validation is done by the jsoncons JSON Schema validator, so the whole schema is enforced:
- ✅ Drafts 4, 6, 7, 2019-09 and 2020-12, chosen by the schema's `$schema` (2020-12 when absent)
- ✅ Nested `properties`, `items`, `additionalProperties`, `required` at any depth
- ✅ `enum`, `const`, `pattern`, `minimum`/`maximum`, `minLength`/`maxLength`, `minItems`/`uniqueItems` and the other value constraints
- ✅ `integer` (whole numbers only), `format` for the standard formats
- ✅ `$ref` within the schema document (`definitions` / `$defs`), `allOf`/`anyOf`/`oneOf`/`not`, `if`/`then`/`else`

`$ref` to another file or URL is not resolved; keep referenced definitions inside the schema file. The validation runs after the operation it is attached to, against the whole file, so attach `SchemaFile` to the last operation on a file (or to each one whose intermediate state must be valid).

> **Upgrading from 7.x:** earlier versions validated only the root type, the root `required` list and the types of top-level properties. Schemas that used to pass may now report violations in nested structures that were previously ignored; those are real mismatches between the file and the schema. Run the install with `JSONEXT_DRYRUN=1` first if in doubt (the validation is skipped in a dry run, but the log shows which operations carry a `SchemaFile`), or check the file with `jsoncli validateSchema`.

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

**Behavior with Multiple Matches:**
- **setValue**: All matched elements are updated with the same value
- **deleteValue**: All matched elements are deleted
- **replaceJsonValue**: All matched elements are replaced with the new JSON value
- **readValue**: Returns the first matched element's value
- **Array operations**: Apply to all matched arrays

This allows for powerful bulk operations. For example, `$..price` will update every `price` property at any depth in the JSON structure.

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

**Filter Examples:**
- `$.store.book[?(@.price < 10)]` - Select all books cheaper than 10
- `$.store.book[?(@.isbn)]` - Select all books that have an ISBN property
- `$.store.book[?(@.category == 'fiction')]` - Select all fiction books

When a filter matches multiple elements, the action is applied to all matching elements.

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

## Common .NET Configuration Patterns

This section provides convenience patterns for typical .NET application configuration scenarios, reducing boilerplate and following .NET conventions.

### Connection Strings

Modern .NET applications commonly store connection strings in the `ConnectionStrings` section of `appsettings.json`:

```json
{
  "ConnectionStrings": {
    "DefaultConnection": "Server=localhost;Database=MyDb;",
    "ReportingConnection": "Server=reporting;Database=Reports;"
  }
}
```

**Setting a default connection string during installation:**

```xml
<Component Id="DatabaseConfig" Guid="{YOUR-GUID}">
  <File Id="AppSettings" Name="appsettings.json" Source="appsettings.json" />
  
  <!-- Set the default connection string from installer properties -->
  <Json:JsonFile 
    Id="SetDefaultConnection" 
    File="[#AppSettings]" 
    ElementPath="$.ConnectionStrings.DefaultConnection" 
    Value="Server=[DB_SERVER];Database=[DB_NAME];User Id=[DB_USER];Password=[DB_PASSWORD];" 
    Action="setValue" />
    <!-- SECURITY WARNING: Avoid storing passwords in plain text. Use Integrated Security or external secret management -->
</Component>
```

**Creating connection strings section if it doesn't exist:**

```xml
<!-- Create the entire ConnectionStrings section with JSONPointer -->
<Json:JsonFile 
  Id="CreateConnectionStrings" 
  File="[#AppSettings]" 
  ElementPath="/ConnectionStrings/DefaultConnection" 
  Value="Server=[DB_SERVER];Database=[DB_NAME];Integrated Security=true;" 
  Action="createJsonPointerValue" />
```

### Logging Configuration

.NET applications use structured logging configuration in `appsettings.json`:

```json
{
  "Logging": {
    "LogLevel": {
      "Default": "Information",
      "Microsoft": "Warning",
      "System": "Warning"
    }
  }
}
```

**Setting log levels during installation:**

```xml
<Component Id="LoggingConfig" Guid="{YOUR-GUID}">
  <File Id="AppSettings" Name="appsettings.json" Source="appsettings.json" />
  
  <!-- Set default log level based on installer property -->
  <Json:JsonFile 
    Id="SetDefaultLogLevel" 
    File="[#AppSettings]" 
    ElementPath="$.Logging.LogLevel.Default" 
    Value="[LOG_LEVEL]" 
    Action="setValue" />
  
  <!-- Configure Microsoft namespace logging -->
  <Json:JsonFile 
    Id="SetMicrosoftLogLevel" 
    File="[#AppSettings]" 
    ElementPath="$.Logging.LogLevel.Microsoft" 
    Value="Warning" 
    Action="setValue" />
</Component>
```

**Common log level values**: `Trace`, `Debug`, `Information`, `Warning`, `Error`, `Critical`, `None`

### Serilog Configuration

For applications using Serilog, you can configure file paths and settings:

```xml
<!-- Configure Serilog file output path -->
<Json:JsonFile 
  Id="SetSerilogPath" 
  File="[#AppSettings]" 
  ElementPath="$.Serilog.WriteTo[\[]0[\]].Args.path" 
  Value="[INSTALLFOLDER]Logs\\application.log" 
  Action="setValue" />

<!-- Set minimum log level -->
<Json:JsonFile 
  Id="SetSerilogMinLevel" 
  File="[#AppSettings]" 
  ElementPath="$.Serilog.MinimumLevel.Default" 
  Value="Information" 
  Action="setValue" />
```

### ASP.NET Core Application Settings

**Setting application URLs:**

```xml
<!-- Configure Kestrel URLs -->
<Json:JsonFile 
  Id="SetApplicationUrl" 
  File="[#AppSettings]" 
  ElementPath="$.Urls" 
  Value="http://localhost:[APP_PORT]" 
  Action="setValue" />
```

**Configuring CORS origins:**

```xml
<!-- Set allowed CORS origins -->
<Json:JsonFile 
  Id="SetCorsOrigins" 
  File="[#AppSettings]" 
  ElementPath="$.Cors.AllowedOrigins" 
  Value='["http://localhost:3000","https://[DOMAIN_NAME]"]' 
  Action="replaceJsonValue" />
```

### Environment-Specific Configuration

.NET applications often use environment-specific configuration files like `appsettings.Production.json`:

```xml
<Component Id="ProductionConfig" Guid="{YOUR-GUID}">
  <File Id="AppSettingsProd" Name="appsettings.Production.json" Source="appsettings.Production.json" />
  
  <!-- Configure production database -->
  <Json:JsonFile 
    Id="SetProdConnection" 
    File="[#AppSettingsProd]" 
    ElementPath="$.ConnectionStrings.DefaultConnection" 
    Value="Server=[PROD_DB_SERVER];Database=[PROD_DB_NAME];Integrated Security=true;" 
    Action="setValue" />
  
  <!-- Set production log level -->
  <Json:JsonFile 
    Id="SetProdLogLevel" 
    File="[#AppSettingsProd]" 
    ElementPath="$.Logging.LogLevel.Default" 
    Value="Warning" 
    Action="setValue" />
</Component>
```

### Service Configuration

**Windows Service settings:**

```xml
<!-- Set service name and display name -->
<Json:JsonFile 
  Id="SetServiceName" 
  File="[#AppSettings]" 
  ElementPath="$.ServiceSettings.ServiceName" 
  Value="[SERVICE_NAME]" 
  Action="setValue" />

<Json:JsonFile 
  Id="SetServiceDisplay" 
  File="[#AppSettings]" 
  ElementPath="$.ServiceSettings.DisplayName" 
  Value="[SERVICE_DISPLAY_NAME]" 
  Action="setValue" />
```

### API Keys and Secrets

**Note**: For production scenarios, consider using secure key storage (Azure Key Vault, AWS Secrets Manager) instead of storing secrets in configuration files.

```xml
<!-- Set API key during installation (use with caution) -->
<Json:JsonFile 
  Id="SetApiKey" 
  File="[#AppSettings]" 
  ElementPath="$.ApiSettings.ApiKey" 
  Value="[API_KEY]" 
  Action="setValue" />
```

### Complete Example: Typical .NET 6+ Application

Here's a complete example combining common patterns:

```xml
<Component Id="DotNetAppConfig" Guid="{YOUR-GUID}">
  <File Id="AppSettings" Name="appsettings.json" Source="appsettings.json" />
  
  <!-- Connection String -->
  <Json:JsonFile 
    Id="SetDefaultConnection" 
    File="[#AppSettings]" 
    ElementPath="$.ConnectionStrings.DefaultConnection" 
    Value="Server=[DB_SERVER];Database=[DB_NAME];Integrated Security=true;" 
    Action="setValue" />
  
  <!-- Logging -->
  <Json:JsonFile 
    Id="SetLogLevel" 
    File="[#AppSettings]" 
    ElementPath="$.Logging.LogLevel.Default" 
    Value="[LOG_LEVEL]" 
    Action="setValue" />
  
  <!-- Application URL -->
  <Json:JsonFile 
    Id="SetAppUrl" 
    File="[#AppSettings]" 
    ElementPath="$.Urls" 
    Value="http://localhost:[APP_PORT]" 
    Action="setValue" />
  
  <!-- Custom Application Path -->
  <Json:JsonFile 
    Id="SetDataPath" 
    File="[#AppSettings]" 
    ElementPath="$.ApplicationSettings.DataPath" 
    Value="[INSTALLFOLDER]Data" 
    Action="setValue" />
</Component>
```

### Best Practices for .NET Configuration

1. **Use installer properties**: Define properties in your WiX project for configurable values, allowing users to customize the installation
2. **Provide defaults**: Always set sensible default values in your source `appsettings.json`
3. **Environment-specific files**: Use separate configuration files for different environments
4. **Sequence operations**: Use the `Sequence` attribute when operations depend on each other
5. **Test paths**: Verify JSONPath expressions work with your JSON structure before deploying
6. **Security**: Never store secrets in plain text; use secure configuration providers in production
7. **Validation**: Use `readValue` action to verify existing configuration before modifying


## Escaping Special Characters

When working with JSONPath in WiX XML files, you need to be aware of multiple layers of escaping:

### 1. MSI Formatting Characters

Square brackets `[` and `]` are special characters in Windows Installer formatted strings. They must be escaped as `[\[]` and `[\]]` in your WiX source files, or, for a `Value` that needs no property expansion, use `Formatted="no"` (see [Property Expansion and Literal Values](#property-expansion-and-literal-values)).

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

## Diagnostics

Three public properties switch on diagnostics for a whole transaction. Pass them on the `msiexec` command line (they are read in the immediate phase and handed to the deferred action, so they work for install, repair and uninstall alike):

| Property | Effect |
|----------|--------|
| `JSONEXT_DRYRUN=1` | Every operation is logged but nothing is written, and no rollback is scheduled |
| `JSONEXT_LOGLEVEL=verbose` | The value at each `ElementPath` is logged before and after every operation |
| `JSONEXT_TRANSFORMLOG=<path>` | A JSON record of every operation is appended to `<path>` |

### Dry Run

```
msiexec /i MyApp.msi /qn /l*v install.log JSONEXT_DRYRUN=1
```

The install runs to completion, files are installed, but every `JsonFile` operation short-circuits after logging what it would have done:

```
WixJsonFile: JSONEXT_DRYRUN is set - JSON operations will be logged but not applied
...
WixJsonFile: DRY RUN - would apply setValue to '$.ConnectionStrings.Default' in 'C:\...\appsettings.json' with value 'Server=...' (flags=2, index=-1)
```

Combine it with `JSONEXT_TRANSFORMLOG` to get the same information as structured JSON, including what each path contained at the time. Use a dry run to check property expansion and paths against a real machine before a rollout, or in CI to prove an installer schedules what you expect. `readValue` operations still run: they modify nothing.

### Verbose Logging

```
msiexec /i MyApp.msi /qn /l*v install.log JSONEXT_LOGLEVEL=verbose
```

adds, for every operation, the JSON at the path before and after it:

```
WixJsonFile: setValue '$.Logging.LogLevel.Default' in 'C:\...\appsettings.json' - before: ["Information"]
WixJsonFile: setValue '$.Logging.LogLevel.Default' in 'C:\...\appsettings.json' - after: ["Warning"] (hr=0x00000000)
```

JSONPath snapshots are the array of matches; `createJsonPointerValue` snapshots are the single value at the pointer. A path with no match reads `<no match>`, a missing file `<file not found>`, and a file that does not parse `<not valid JSON>`. Snapshots are truncated at 2 KB. The same lines are also written whenever the MSI log itself is verbose (`/l*v`, or `MsiLogging` containing `v`), so `JSONEXT_LOGLEVEL=verbose` only matters for a non-verbose log.

### Transform Log

```
msiexec /i MyApp.msi /qn JSONEXT_TRANSFORMLOG=C:\Logs\MyApp-json.log
```

writes a JSON array with one entry per operation, appended as each operation completes (so it is complete up to the point of a failure, and is not undone by rollback):

```json
[
  {
    "timestamp": "2026-09-13T15:20:36Z",
    "phase": "install",
    "file": "C:\\Program Files\\MyApp\\appsettings.json",
    "action": "setValue",
    "elementPath": "$.expensive",
    "value": "15",
    "index": -1,
    "flags": 1026,
    "outcome": "applied",
    "hresult": "0x00000000",
    "before": [10],
    "after": [15]
  }
]
```

`outcome` is one of `applied`, `skipped` (an `OnlyIfExists` miss), `dry-run` or `failed`; `after` is omitted when nothing was written. The same file can be reused across transactions: entries from an uninstall carry `"phase": "uninstall"`. A transform log that cannot be written is reported in the MSI log and never fails the install.

### Command-Line Harness (jsoncli)

The NuGet package ships `tools/jsoncli.exe`, a console harness that runs the exact transform code of the custom action against a JSON file, outside of any MSI. Use it to try an `ElementPath` against a sample file, preview an action, or reproduce an operation from a transform log:

```
jsoncli <action> <jsonFile> <elementPath> [value] [--index n] [--schema file] [--only-if-exists] [--dry-run] [--quiet]
jsoncli readValue <jsonFile> <elementPath> [--default value]
jsoncli validateSchema <jsonFile> <schemaFile>
```

```
> jsoncli setValue appsettings.json $.Logging.LogLevel.Default Warning
setValue $.Logging.LogLevel.Default in appsettings.json
  before:  ["Information"]
  after:   ["Warning"]
  outcome: applied (hr=0x00000000)
```

Values and paths are taken literally: `[PROPERTY]` references are not expanded and brackets need no MSI escaping (`$.a[0]`, not `$.a[\[]0[\]]`). The exit code is 0 on success, 1 when the operation fails and 2 for a usage error, so it can gate a CI job. The tool lives in the package's `tools` folder (for example `%USERPROFILE%\.nuget\packages\wixjsonfileextension\<version>\tools`).

## Troubleshooting

### Common Issues

1. **"Element not found" errors**
   - Verify your JSONPath is correct using online JSONPath evaluators like [JSONPath Online Evaluator](https://jsonpath.com/)
   - Check that square brackets are properly escaped: `[\[]` and `[\]]`
   - Ensure the JSON file is valid and well-formed
   - Look for "WixJsonFile: Error" messages in the MSI log for specific path details

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
   - The compiler will now warn you about lowercase property references

5. **Build-time warnings**
   - The extension now provides compile-time diagnostics for common issues:
     - **Unescaped brackets**: Warns if square brackets aren't properly escaped as `[\[]` and `[\]]`
     - **Invalid JSONPath syntax**: Detects basic syntax errors in ElementPath
     - **Missing required attributes**: Ensures `Value` is present for setValue/replaceJsonValue actions
     - **Property name format**: Warns if property names aren't uppercase

### Debugging Tips

- Run the install with `JSONEXT_LOGLEVEL=verbose` and `JSONEXT_TRANSFORMLOG=<path>` to see exactly what each operation found and wrote, or `JSONEXT_DRYRUN=1` to see what it would do without changing anything (see [Diagnostics](#diagnostics)).
- Reproduce a single operation outside MSI with `tools\jsoncli.exe` from the package.

- Use the `readValue` action to verify paths are correct before modifying
- Test your JSONPath expressions in an online evaluator like [jsonpath.com](https://jsonpath.com/)
- For complex queries, test with the jsoncons library directly or use its online playground
- Check the MSI log file for detailed error messages: `msiexec /i YourInstaller.msi /l*v install.log`
- Search the log for `WixJsonFile:` to find all JSON-related operations and errors
- All error messages now include the affected file path and element path for easier debugging
- Refer to the example in `TestJsonConfigInstaller/Product.wxs` for working patterns
- Check the [Cookbook](docs/COOKBOOK.md) for common patterns and best practices
- When using schema validation, test your JSON against the schema separately first using tools like [jsonschemavalidator.net](https://www.jsonschemavalidator.net/)

### Understanding Error Messages

The extension now provides structured error messages with the format:
```
WixJsonFile: Error - [Description] for path '[ElementPath]' in file '[FilePath]'
```

Common error messages:
- `File not found` - The specified JSON file doesn't exist at the given path
- `No elements found at path` - The JSONPath query didn't match any elements
- `Invalid element path parameter` - The ElementPath attribute is missing or empty
- `Failed to parse JSON value` - The Value contains invalid JSON (for replaceJsonValue action)

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
