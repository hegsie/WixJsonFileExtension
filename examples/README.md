# WixJsonFileExtension Examples

This directory contains reusable WiX fragment examples for common JSON configuration scenarios.

## Available Examples

### 1. ConnectionStrings.wxs
Configure database connection strings during installation.

**Scenarios covered:**
- Single connection string updates
- Multiple connection strings (Primary, Reporting, Logging)
- Property-based configuration
- Custom action integration

**Usage:**
```xml
<ComponentGroupRef Id="ConnectionStringComponents" />
```

### 2. LoggingConfiguration.wxs
Configure logging settings including log levels and file paths.

**Scenarios covered:**
- Basic .NET logging levels (Default, Microsoft, System)
- Serilog configuration
- Log file path configuration
- Rolling interval settings
- Environment-based log level overrides

**Usage:**
```xml
<ComponentGroupRef Id="LoggingComponents" />
```

### 3. FeatureFlags.wxs
Toggle feature flags based on installation options and environment.

**Scenarios covered:**
- Simple boolean feature flags
- Edition-based features (Standard, Premium, Enterprise)
- Environment-based features (Development, Production)
- Beta feature enablement
- API and performance feature flags

**Usage:**
```xml
<ComponentGroupRef Id="FeatureFlagComponents" />
```

### 4. ApiEndpoints.wxs
Configure API endpoint URLs, timeouts, and authentication.

**Scenarios covered:**
- API base URL configuration
- Timeout and retry settings
- Authentication configuration (Bearer, ApiKey)
- Endpoint overrides
- Environment-specific API URLs

**Usage:**
```xml
<ComponentGroupRef Id="ApiEndpointComponents" />
```

### 5. EnvironmentConfiguration.wxs
Complete environment-specific configuration (Development, Staging, Production).

**Scenarios covered:**
- Environment detection and setting
- Per-environment connection strings
- Per-environment logging configuration
- Per-environment API settings
- Feature enablement by environment

**Usage:**
```xml
<ComponentGroupRef Id="EnvironmentConfigComponents" />
```

### 6. AdvancedArrayOperations.wxs ⭐ NEW
Advanced array manipulation and conditional updates.

**Scenarios covered:**
- Removing duplicates with `distinctValues` action
- Conditional updates with `OnlyIfExists` attribute
- Complex JSONPath filters for bulk operations
- Multi-select queries affecting multiple elements
- Combining filters with array operations
- Advanced filter examples (comparison operators, logical AND/OR)

**Usage:**
```xml
<ComponentGroupRef Id="AdvancedArrayOperationsComponents" />
```

**Features demonstrated:**
- Remove duplicates from arrays (tags, feature flags, CORS origins)
- Conditional updates that only modify existing values
- Multi-select queries (`$..timeout` updates all timeouts)
- Complex filters (`$.endpoints[?(@.environment == 'production')]`)
- Bulk operations with filters (`$.features[?(@.priority > 5)].enabled`)
- Combining multiple operations in sequence

### 7. CompositeElements.wxs ⭐ NEW
High-level composite elements and JsonTransaction grouping for cleaner authoring.

**Scenarios covered:**
- Using `Json:AppSettings` for .NET application settings
- Using `Json:ConnectionString` for database connections
- Using `Json:LoggingLevel` for logging configuration
- Grouping operations with `Json:JsonTransaction`
- Combining composite elements with regular JsonFile elements
- Real-world complete application setup examples

**Usage:**
```xml
<ComponentGroupRef Id="CompositeElementsExamples" />
```

**Features demonstrated:**
- Simplified syntax with composite elements
- Automatic sequence assignment in transactions
- Grouping related configuration changes together
- Mixing high-level and low-level elements
- Production-ready configuration patterns
- Cleaner, more maintainable WiX authoring

**Key Benefits:**
- 📝 **Less verbose** - Composite elements reduce boilerplate
- 🗂️ **Better organization** - JsonTransaction groups related operations
- 🎯 **Self-documenting** - Element names clearly indicate purpose
- ✅ **Type safety** - Specific attributes for common patterns
- 📚 **Best practices** - Follows .NET conventions automatically


## How to Use These Examples

### Method 1: Direct Inclusion

Include the fragment file in your main WiX source file:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs"
     xmlns:Json="http://schemas.hegsie.com/wix/JsonExtension">
  
  <Package Name="MyApplication" ...>
    <Feature Id="MainFeature" Level="1">
      <ComponentGroupRef Id="ConnectionStringComponents" />
      <ComponentGroupRef Id="LoggingComponents" />
    </Feature>
  </Package>
  
  <!-- Include the fragment files -->
  <?include ConnectionStrings.wxs ?>
  <?include LoggingConfiguration.wxs ?>
  
</Wix>
```

### Method 2: Compile Separately

Compile the fragments into wixobj files and link them:

```bash
# Compile fragments
wix build -o ConnectionStrings.wixobj ConnectionStrings.wxs
wix build -o LoggingConfiguration.wixobj LoggingConfiguration.wxs

# Link with your main installer
wix build -o MyInstaller.msi Product.wxs ConnectionStrings.wixobj LoggingConfiguration.wixobj
```

### Method 3: Copy and Customize

Copy the relevant portions into your own WiX source file and customize as needed:

1. Copy the `<ComponentGroup>` section
2. Copy the `<Property>` definitions
3. Modify IDs, file references, and values to match your application
4. Adjust JSONPath expressions to match your JSON structure

## Required Namespace

All examples require the JSON extension namespace:

```xml
<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs"
     xmlns:Json="http://schemas.hegsie.com/wix/JsonExtension">
```

## Property Definitions

Each example includes property definitions with default values. You can override these in your main installer:

```xml
<!-- Override example property -->
<Property Id="DB_CONNECTION_STRING" Value="Your custom value" />

<!-- Set from command line -->
<!-- msiexec /i YourInstaller.msi DB_CONNECTION_STRING="Server=myserver;..." -->
```

## Common Customizations

### Change File Reference

Update the File element and references:

```xml
<!-- Original -->
<File Id="AppSettingsJsonConn" Name="appsettings.json" Source="appsettings.json" />
<Json:JsonFile File="[#AppSettingsJsonConn]" ... />

<!-- Customized -->
<File Id="MyConfigFile" Name="config.json" Source="path/to/config.json" />
<Json:JsonFile File="[#MyConfigFile]" ... />
```

### Change JSONPath

Update the ElementPath to match your JSON structure:

```xml
<!-- Original -->
ElementPath="$.ConnectionStrings.DefaultConnection"

<!-- Customized for different structure -->
ElementPath="$.Database.Connection"
```

### Add Conditions

Make updates conditional based on properties:

```xml
<Json:JsonFile ... >
  <Condition><![CDATA[MY_PROPERTY = "1"]]></Condition>
</Json:JsonFile>
```

### Adjust Sequence

Control the order of operations with the Sequence attribute:

```xml
<Json:JsonFile Sequence="1" ... />  <!-- Runs first -->
<Json:JsonFile Sequence="2" ... />  <!-- Runs second -->
```

## JSON File Structure Examples

### Sample appsettings.json for Connection Strings

```json
{
  "ConnectionStrings": {
    "DefaultConnection": "Server=localhost;Database=MyApp;",
    "Primary": "Server=primary;Database=MyApp;",
    "Reporting": "Server=reporting;Database=MyApp;"
  }
}
```

### Sample appsettings.json for Logging

```json
{
  "Logging": {
    "LogLevel": {
      "Default": "Information",
      "Microsoft": "Warning",
      "System": "Warning"
    }
  },
  "Serilog": {
    "MinimumLevel": {
      "Default": "Information"
    },
    "WriteTo": [
      {
        "Name": "File",
        "Args": {
          "path": "Logs/app.log",
          "rollingInterval": "Day"
        }
      }
    ]
  }
}
```

### Sample appsettings.json for Feature Flags

```json
{
  "Features": {
    "EnableNewUI": false,
    "EnableAdvancedReporting": false,
    "EnableBetaFeatures": false,
    "EnableSwagger": false,
    "EnableCaching": true
  }
}
```

## Additional Resources

- [Cookbook Documentation](../docs/COOKBOOK.md) - Detailed patterns and best practices
- [Main README](../README.md) - Complete extension documentation
- [WiX Toolset Documentation](https://wixtoolset.org/docs/)

## Tips

1. **Always test with sample JSON files** before deploying to production
2. **Use Sequence attributes** to ensure proper operation order
3. **Escape square brackets** in JSONPath: `[\[]0[\]]` not `[0]`
4. **Validate properties** exist before using them in conditions
5. **Use descriptive IDs** for easier debugging in MSI logs
6. **Comment your customizations** for future maintainability

## Contributing

Found an issue or have a suggestion for a new example? Please contribute:

1. Create a new example following the existing patterns
2. Include comprehensive comments
3. Provide sample JSON structure
4. Document any required properties
5. Submit a pull request

---

For more information, see the [main documentation](../README.md).
