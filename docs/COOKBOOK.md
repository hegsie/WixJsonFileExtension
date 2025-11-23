# WixJsonFileExtension Cookbook

This cookbook provides practical examples and patterns for common JSON configuration scenarios using WixJsonFileExtension with WiX 5+.

## Table of Contents

1. [Connection Strings](#connection-strings)
2. [Logging Configuration](#logging-configuration)
3. [Feature Flags](#feature-flags)
4. [API Endpoints](#api-endpoints)
5. [Environment-Specific Settings](#environment-specific-settings)
6. [Complex Nested Configurations](#complex-nested-configurations)
7. [Array Manipulation](#array-manipulation)
8. [Conditional Updates](#conditional-updates)

---

## Connection Strings

### Pattern: Update Database Connection String

**Use Case**: Update the database connection string during installation based on user input.

**Example appsettings.json**:
```json
{
  "ConnectionStrings": {
    "DefaultConnection": "Server=localhost;Database=MyApp;Trusted_Connection=True;"
  }
}
```

**WiX Fragment**:
```xml
<Component Id="ConfigComponent" Guid="*">
  <File Id="AppSettingsJson" Name="appsettings.json" Source="appsettings.json" />
  
  <!-- Update connection string from user property -->
  <Json:JsonFile 
    Id="UpdateConnectionString" 
    File="[#AppSettingsJson]" 
    ElementPath="$.ConnectionStrings.DefaultConnection" 
    Value="[DB_CONNECTION_STRING]" 
    Action="setValue" />
</Component>
```

**Installer Properties**:
```xml
<!-- Define property with default value -->
<Property Id="DB_CONNECTION_STRING" Value="Server=localhost;Database=MyApp;Trusted_Connection=True;" />

<!-- Collect from user via UI -->
<UI>
  <Dialog Id="DatabaseConfigDlg" Width="370" Height="270" Title="Database Configuration">
    <Control Id="ConnectionStringEdit" Type="Edit" X="20" Y="60" Width="330" Height="18" 
             Property="DB_CONNECTION_STRING" />
  </Dialog>
</UI>
```

### Pattern: Multiple Connection Strings

**Example appsettings.json**:
```json
{
  "ConnectionStrings": {
    "Primary": "Server=primary;Database=MyApp;",
    "Reporting": "Server=reporting;Database=MyApp;",
    "Logging": "Server=logging;Database=Logs;"
  }
}
```

**WiX Fragment**:
```xml
<Component Id="ConfigComponent" Guid="*">
  <File Id="AppSettingsJson" Name="appsettings.json" Source="appsettings.json" />
  
  <Json:JsonFile Id="UpdatePrimaryDB" File="[#AppSettingsJson]" 
    ElementPath="$.ConnectionStrings.Primary" 
    Value="[PRIMARY_DB_CONNECTION]" 
    Action="setValue" Sequence="1" />
  
  <Json:JsonFile Id="UpdateReportingDB" File="[#AppSettingsJson]" 
    ElementPath="$.ConnectionStrings.Reporting" 
    Value="[REPORTING_DB_CONNECTION]" 
    Action="setValue" Sequence="2" />
  
  <Json:JsonFile Id="UpdateLoggingDB" File="[#AppSettingsJson]" 
    ElementPath="$.ConnectionStrings.Logging" 
    Value="[LOGGING_DB_CONNECTION]" 
    Action="setValue" Sequence="3" />
</Component>
```

---

## Logging Configuration

### Pattern: Configure Log Level

**Use Case**: Set the logging level based on installation mode (Debug/Production).

**Example appsettings.json**:
```json
{
  "Logging": {
    "LogLevel": {
      "Default": "Information",
      "Microsoft": "Warning",
      "Microsoft.Hosting.Lifetime": "Information"
    }
  }
}
```

**WiX Fragment**:
```xml
<Component Id="ConfigComponent" Guid="*">
  <File Id="AppSettingsJson" Name="appsettings.json" Source="appsettings.json" />
  
  <!-- Set default log level -->
  <Json:JsonFile 
    Id="SetDefaultLogLevel" 
    File="[#AppSettingsJson]" 
    ElementPath="$.Logging.LogLevel.Default" 
    Value="[LOG_LEVEL]" 
    Action="setValue" />
  
  <!-- Set Microsoft log level -->
  <Json:JsonFile 
    Id="SetMicrosoftLogLevel" 
    File="[#AppSettingsJson]" 
    ElementPath="$.Logging.LogLevel.Microsoft" 
    Value="Warning" 
    Action="setValue" />
</Component>

<!-- Property defaults based on build configuration -->
<Property Id="LOG_LEVEL" Value="Information" />

<!-- Set to Debug for development installs -->
<SetProperty Id="LOG_LEVEL" Value="Debug" After="AppSearch" Sequence="first">
  <![CDATA[DEVELOPMENT_MODE = "1"]]>
</SetProperty>
```

### Pattern: Configure File Logging Path

**Use Case**: Set custom log file path during installation.

**Example appsettings.json**:
```json
{
  "Serilog": {
    "WriteTo": [
      {
        "Name": "File",
        "Args": {
          "path": "C:\\Logs\\app.log",
          "rollingInterval": "Day"
        }
      }
    ]
  }
}
```

**WiX Fragment**:
```xml
<Component Id="ConfigComponent" Guid="*">
  <File Id="AppSettingsJson" Name="appsettings.json" Source="appsettings.json" />
  
  <!-- Update log file path with proper escaping for backslashes -->
  <Json:JsonFile 
    Id="SetLogPath" 
    File="[#AppSettingsJson]" 
    ElementPath="$.Serilog.WriteTo[\[]0[\]].Args.path" 
    Value="[LOGFOLDER]app-.log" 
    Action="setValue" />
  
  <!-- Update rolling interval -->
  <Json:JsonFile 
    Id="SetLogRolling" 
    File="[#AppSettingsJson]" 
    ElementPath="$.Serilog.WriteTo[\[]0[\]].Args.rollingInterval" 
    Value="[LOG_ROLLING_INTERVAL]" 
    Action="setValue" />
</Component>

<!-- Define log folder property -->
<Property Id="LOGFOLDER" Value="C:\\ProgramData\\[Manufacturer]\\[ProductName]\\Logs\\" />
<Property Id="LOG_ROLLING_INTERVAL" Value="Day" />
```

---

## Feature Flags

### Pattern: Toggle Feature Flags

**Use Case**: Enable or disable features based on installation options.

**Example appsettings.json**:
```json
{
  "Features": {
    "EnableNewUI": false,
    "EnableAdvancedReporting": false,
    "EnableBetaFeatures": false
  }
}
```

**WiX Fragment**:
```xml
<Component Id="ConfigComponent" Guid="*">
  <File Id="AppSettingsJson" Name="appsettings.json" Source="appsettings.json" />
  
  <!-- Enable new UI feature if selected -->
  <Json:JsonFile 
    Id="EnableNewUIFeature" 
    File="[#AppSettingsJson]" 
    ElementPath="$.Features.EnableNewUI" 
    Value="true" 
    Action="setValue">
    <Condition><![CDATA[FEATURE_NEW_UI = "1"]]></Condition>
  </Json:JsonFile>
  
  <!-- Enable advanced reporting if premium edition -->
  <Json:JsonFile 
    Id="EnableAdvancedReporting" 
    File="[#AppSettingsJson]" 
    ElementPath="$.Features.EnableAdvancedReporting" 
    Value="true" 
    Action="setValue">
    <Condition><![CDATA[EDITION = "Premium"]]></Condition>
  </Json:JsonFile>
</Component>

<!-- Feature selection properties -->
<Property Id="FEATURE_NEW_UI" Value="0" />
<Property Id="EDITION" Value="Standard" />
```

### Pattern: Environment-Based Feature Flags

**WiX Fragment**:
```xml
<Component Id="ConfigComponent" Guid="*">
  <File Id="AppSettingsJson" Name="appsettings.json" Source="appsettings.json" />
  
  <!-- Enable beta features only in development -->
  <Json:JsonFile 
    Id="EnableBetaFeatures" 
    File="[#AppSettingsJson]" 
    ElementPath="$.Features.EnableBetaFeatures" 
    Value="true" 
    Action="setValue">
    <Condition><![CDATA[ENVIRONMENT = "Development"]]></Condition>
  </Json:JsonFile>
  
  <!-- Disable in production (ensure it's false) -->
  <Json:JsonFile 
    Id="DisableBetaFeatures" 
    File="[#AppSettingsJson]" 
    ElementPath="$.Features.EnableBetaFeatures" 
    Value="false" 
    Action="setValue">
    <Condition><![CDATA[ENVIRONMENT = "Production"]]></Condition>
  </Json:JsonFile>
</Component>
```

---

## API Endpoints

### Pattern: Configure API URLs

**Use Case**: Set API endpoint URLs based on environment or user input.

**Example appsettings.json**:
```json
{
  "ApiSettings": {
    "BaseUrl": "https://api.example.com",
    "Endpoints": {
      "Users": "/api/v1/users",
      "Products": "/api/v1/products"
    },
    "Timeout": 30
  }
}
```

**WiX Fragment**:
```xml
<Component Id="ConfigComponent" Guid="*">
  <File Id="AppSettingsJson" Name="appsettings.json" Source="appsettings.json" />
  
  <!-- Set API base URL -->
  <Json:JsonFile 
    Id="SetApiBaseUrl" 
    File="[#AppSettingsJson]" 
    ElementPath="$.ApiSettings.BaseUrl" 
    Value="[API_BASE_URL]" 
    Action="setValue" />
  
  <!-- Set API timeout -->
  <Json:JsonFile 
    Id="SetApiTimeout" 
    File="[#AppSettingsJson]" 
    ElementPath="$.ApiSettings.Timeout" 
    Value="[API_TIMEOUT]" 
    Action="setValue" />
</Component>

<!-- Default API settings -->
<Property Id="API_BASE_URL" Value="https://api.example.com" />
<Property Id="API_TIMEOUT" Value="30" />

<!-- Environment-specific overrides -->
<SetProperty Id="API_BASE_URL" Value="https://dev-api.example.com" After="AppSearch" Sequence="first">
  <![CDATA[ENVIRONMENT = "Development"]]>
</SetProperty>

<SetProperty Id="API_BASE_URL" Value="https://staging-api.example.com" After="AppSearch" Sequence="first">
  <![CDATA[ENVIRONMENT = "Staging"]]>
</SetProperty>
```

---

## Environment-Specific Settings

### Pattern: Complete Environment Configuration

**Use Case**: Configure all settings based on deployment environment.

**Example appsettings.json**:
```json
{
  "Environment": "Development",
  "ConnectionStrings": {
    "Default": "Server=localhost;Database=MyApp;"
  },
  "Logging": {
    "LogLevel": {
      "Default": "Information"
    }
  },
  "Features": {
    "DebugMode": false
  }
}
```

**WiX Fragment**:
```xml
<Component Id="ConfigComponent" Guid="*">
  <File Id="AppSettingsJson" Name="appsettings.json" Source="appsettings.json" />
  
  <!-- Set environment name -->
  <Json:JsonFile 
    Id="SetEnvironment" 
    File="[#AppSettingsJson]" 
    ElementPath="$.Environment" 
    Value="[ENVIRONMENT]" 
    Action="setValue" 
    Sequence="1" />
  
  <!-- Development environment settings -->
  <Json:JsonFile 
    Id="DevConnectionString" 
    File="[#AppSettingsJson]" 
    ElementPath="$.ConnectionStrings.Default" 
    Value="Server=dev-db;Database=MyApp;" 
    Action="setValue" 
    Sequence="2">
    <Condition><![CDATA[ENVIRONMENT = "Development"]]></Condition>
  </Json:JsonFile>
  
  <Json:JsonFile 
    Id="DevLogLevel" 
    File="[#AppSettingsJson]" 
    ElementPath="$.Logging.LogLevel.Default" 
    Value="Debug" 
    Action="setValue" 
    Sequence="3">
    <Condition><![CDATA[ENVIRONMENT = "Development"]]></Condition>
  </Json:JsonFile>
  
  <!-- Production environment settings -->
  <Json:JsonFile 
    Id="ProdConnectionString" 
    File="[#AppSettingsJson]" 
    ElementPath="$.ConnectionStrings.Default" 
    Value="[PROD_CONNECTION_STRING]" 
    Action="setValue" 
    Sequence="2">
    <Condition><![CDATA[ENVIRONMENT = "Production"]]></Condition>
  </Json:JsonFile>
  
  <Json:JsonFile 
    Id="ProdLogLevel" 
    File="[#AppSettingsJson]" 
    ElementPath="$.Logging.LogLevel.Default" 
    Value="Warning" 
    Action="setValue" 
    Sequence="3">
    <Condition><![CDATA[ENVIRONMENT = "Production"]]></Condition>
  </Json:JsonFile>
</Component>

<!-- Environment property -->
<Property Id="ENVIRONMENT" Value="Production" />
<Property Id="PROD_CONNECTION_STRING" />
```

---

## Complex Nested Configurations

### Pattern: Deep Nesting with Arrays

**Use Case**: Configure complex nested settings like Serilog with multiple sinks.

**Example appsettings.json**:
```json
{
  "Serilog": {
    "Using": ["Serilog.Sinks.Console", "Serilog.Sinks.File"],
    "MinimumLevel": "Debug",
    "WriteTo": [
      {
        "Name": "Console"
      },
      {
        "Name": "File",
        "Args": {
          "path": "Logs/app.log",
          "rollingInterval": "Day",
          "outputTemplate": "{Timestamp:yyyy-MM-dd HH:mm:ss.fff zzz} [{Level:u3}] {Message:lj}{NewLine}{Exception}"
        }
      }
    ]
  }
}
```

**WiX Fragment**:
```xml
<Component Id="ConfigComponent" Guid="*">
  <File Id="AppSettingsJson" Name="appsettings.json" Source="appsettings.json" />
  
  <!-- Update minimum level -->
  <Json:JsonFile 
    Id="SetMinLevel" 
    File="[#AppSettingsJson]" 
    ElementPath="$.Serilog.MinimumLevel" 
    Value="[LOG_MIN_LEVEL]" 
    Action="setValue" />
  
  <!-- Update file sink path (second item in WriteTo array) -->
  <Json:JsonFile 
    Id="SetFileSinkPath" 
    File="[#AppSettingsJson]" 
    ElementPath="$.Serilog.WriteTo[\[]1[\]].Args.path" 
    Value="[LOGFOLDER]app.log" 
    Action="setValue" />
  
  <!-- Update rolling interval -->
  <Json:JsonFile 
    Id="SetRollingInterval" 
    File="[#AppSettingsJson]" 
    ElementPath="$.Serilog.WriteTo[\[]1[\]].Args.rollingInterval" 
    Value="[LOG_ROLLING_INTERVAL]" 
    Action="setValue" />
  
  <!-- Update output template -->
  <Json:JsonFile 
    Id="SetOutputTemplate" 
    File="[#AppSettingsJson]" 
    ElementPath="$.Serilog.WriteTo[\[]1[\]].Args.outputTemplate" 
    Value="{Timestamp:yyyy-MM-dd HH:mm:ss} [{Level}] {Message}{NewLine}{Exception}" 
    Action="setValue" />
</Component>

<Property Id="LOG_MIN_LEVEL" Value="Information" />
<Property Id="LOGFOLDER" Value="[CommonAppDataFolder][Manufacturer]\\[ProductName]\\Logs\\" />
<Property Id="LOG_ROLLING_INTERVAL" Value="Day" />
```

---

## Array Manipulation

### Pattern: Update Multiple Array Items

**Use Case**: Update all items in an array matching a condition.

**Example appsettings.json**:
```json
{
  "Servers": [
    {
      "Name": "WebServer1",
      "Url": "http://localhost:5000",
      "Enabled": true
    },
    {
      "Name": "WebServer2",
      "Url": "http://localhost:5001",
      "Enabled": false
    }
  ]
}
```

**WiX Fragment**:
```xml
<Component Id="ConfigComponent" Guid="*">
  <File Id="AppSettingsJson" Name="appsettings.json" Source="appsettings.json" />
  
  <!-- Update first server URL -->
  <Json:JsonFile 
    Id="UpdateServer1Url" 
    File="[#AppSettingsJson]" 
    ElementPath="$.Servers[\[]0[\]].Url" 
    Value="[SERVER1_URL]" 
    Action="setValue" />
  
  <!-- Update second server URL -->
  <Json:JsonFile 
    Id="UpdateServer2Url" 
    File="[#AppSettingsJson]" 
    ElementPath="$.Servers[\[]1[\]].Url" 
    Value="[SERVER2_URL]" 
    Action="setValue" />
  
  <!-- Update server by name using filter -->
  <Json:JsonFile 
    Id="UpdateServerByName" 
    File="[#AppSettingsJson]" 
    ElementPath="$.Servers[\[]?(@.Name == 'WebServer1')[\]].Enabled" 
    Value="true" 
    Action="setValue" />
</Component>

<Property Id="SERVER1_URL" Value="http://localhost:5000" />
<Property Id="SERVER2_URL" Value="http://localhost:5001" />
```

---

## Conditional Updates

### Pattern: Conditional JSON Modifications

**Use Case**: Only update certain values if specific conditions are met.

**WiX Fragment**:
```xml
<Component Id="ConfigComponent" Guid="*">
  <File Id="AppSettingsJson" Name="appsettings.json" Source="appsettings.json" />
  
  <!-- Only update if user selected custom database -->
  <Json:JsonFile 
    Id="CustomDatabaseConnection" 
    File="[#AppSettingsJson]" 
    ElementPath="$.ConnectionStrings.Default" 
    Value="[CUSTOM_DB_CONNECTION]" 
    Action="setValue">
    <Condition><![CDATA[USE_CUSTOM_DATABASE = "1" AND CUSTOM_DB_CONNECTION]]></Condition>
  </Json:JsonFile>
  
  <!-- Enable HTTPS only if certificate is configured -->
  <Json:JsonFile 
    Id="EnableHttps" 
    File="[#AppSettingsJson]" 
    ElementPath="$.Kestrel.Endpoints.Https.Enabled" 
    Value="true" 
    Action="setValue">
    <Condition><![CDATA[SSL_CERTIFICATE_PATH <> ""]]></Condition>
  </Json:JsonFile>
  
  <!-- Set certificate path if provided -->
  <Json:JsonFile 
    Id="SetCertPath" 
    File="[#AppSettingsJson]" 
    ElementPath="$.Kestrel.Endpoints.Https.Certificate.Path" 
    Value="[SSL_CERTIFICATE_PATH]" 
    Action="setValue">
    <Condition><![CDATA[SSL_CERTIFICATE_PATH <> ""]]></Condition>
  </Json:JsonFile>
</Component>
```

---

## Best Practices

### 1. **Use Sequence Attribute**
When making multiple changes, use the `Sequence` attribute to control the order of operations:
```xml
<Json:JsonFile Id="CreateSection" ... Sequence="1" />
<Json:JsonFile Id="UpdateValue" ... Sequence="2" />
```

### 2. **Escape Square Brackets**
Always escape square brackets in JSONPath expressions for MSI:
```xml
<!-- Wrong -->
ElementPath="$.Books[0].Title"

<!-- Correct -->
ElementPath="$.Books[\[]0[\]].Title"
```

### 3. **Use File References**
Reference files using `[#FileId]` instead of hardcoding paths:
```xml
<File Id="AppConfig" Name="appsettings.json" Source="appsettings.json" />
<Json:JsonFile File="[#AppConfig]" ... />
```

### 4. **Validate Property Names**
Property names must be uppercase and set before use:
```xml
<Property Id="MY_VALUE" Value="Default" />
<Json:JsonFile Value="[MY_VALUE]" ... />
```

### 5. **Use Conditions for Optional Updates**
Wrap optional updates in conditions:
```xml
<Json:JsonFile ...>
  <Condition><![CDATA[PROPERTY_NAME <> ""]]></Condition>
</Json:JsonFile>
```

### 6. **Create Before Update**
Use `createJsonPointerValue` to create paths that might not exist:
```xml
<Json:JsonFile 
  ElementPath="/NewSection/NewValue" 
  Value="MyValue" 
  Action="createJsonPointerValue" />
```

### 7. **Handle Backslashes in Paths**
Double backslashes for Windows paths:
```xml
Value="C:\\Program Files\\MyApp\\config.json"
```

---

## Debugging Tips

### View MSI Log
Generate a detailed log file to troubleshoot JSON file operations:
```cmd
msiexec /i YourInstaller.msi /l*v install.log
```

### Search for JSON Operations
Look for these patterns in the log:
- `ExecJsonFile` - Custom action execution
- `Configuring JSON file` - File being processed
- `Element path:` - The JSONPath being used
- `Setting JSON value` - Value operations
- `Error` or `Failed` - Error messages

### Test JSONPath Expressions
Use online JSONPath evaluators to test your expressions:
- https://jsonpath.com/
- https://jsonpath.curiousconcept.com/

### Common Errors and Solutions

**Error: "Element not found"**
- Check JSONPath syntax
- Verify the path exists in your JSON
- Ensure square brackets are escaped: `[\[]` and `[\]]`

**Error: "Invalid file path"**
- Verify file reference: `[#FileId]`
- Check file is in the same component
- Ensure file is installed before modification

**Property not expanded**
- Property must be uppercase
- Property must be set before JSON action executes
- Use brackets: `[PROPERTY_NAME]`

---

## Additional Resources

- [Main Documentation](../README.md)
- [Example Fragments](../examples/)
- [JSONPath Specification](https://goessner.net/articles/JsonPath/)
- [JSONPointer Specification](https://tools.ietf.org/html/rfc6901)
- [WiX Toolset Documentation](https://wixtoolset.org/docs/)
