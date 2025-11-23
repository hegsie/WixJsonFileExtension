# XSD Schema and IntelliSense Setup

The WixJsonFileExtension includes a comprehensive XSD schema file that provides rich IntelliSense support in IDEs like Visual Studio and VS Code.

## What's Included

The `json.xsd` schema file provides:
- **Detailed attribute descriptions** - Hover tooltips for every element and attribute
- **Strict typing for Action attribute** - Enumeration with descriptions for each action type
- **Default value specifications** - Action defaults to "setValue", OnlyIfExists defaults to "no"
- **Validation at design time** - Catch errors before building
- **Auto-completion** - IntelliSense for all elements and attributes
- **Context-sensitive help** - Understand what each attribute does while authoring

## Using the Schema

### Automatic Usage

The schema is automatically available when you reference the extension namespace in your WiX source files:

```xml
<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs"
     xmlns:Json="http://schemas.hegsie.com/wix/JsonExtension">
  
  <!-- IntelliSense will work for all Json: prefixed elements -->
  <Json:JsonFile ... />
  <Json:ConnectionString ... />
  <Json:AppSettings ... />
</Wix>
```

### Visual Studio Setup

Visual Studio automatically discovers XSD schemas from referenced NuGet packages. No additional setup is required.

### VS Code Setup

For VS Code, you need to install the [Red Hat XML extension](https://marketplace.visualstudio.com/items?itemName=redhat.vscode-xml) and configure it to use the schema.

1. **Install the XML extension:**
   ```
   code --install-extension redhat.vscode-xml
   ```

2. **Locate the schema file:**
   The schema is included in the NuGet package under `schemas/json.xsd`. After adding the package reference, it will be in:
   ```
   ~/.nuget/packages/wixjsonfileextension/{version}/schemas/json.xsd
   ```

3. **Configure VS Code:**
   Add an XML catalog to your workspace `.vscode/settings.json`:
   
   ```json
   {
     "xml.catalogs": [
       "path/to/catalog.xml"
     ]
   }
   ```

4. **Create catalog.xml:**
   ```xml
   <?xml version="1.0" encoding="UTF-8"?>
   <catalog xmlns="urn:oasis:names:tc:entity:xmlns:xml:catalog">
     <uri 
       name="http://schemas.hegsie.com/wix/JsonExtension" 
       uri="~/.nuget/packages/wixjsonfileextension/6.0.0/schemas/json.xsd" />
   </catalog>
   ```

## Schema Elements

### JsonFile Element

The core element for JSON file manipulation. IntelliSense provides:
- **Action attribute**: Enumeration with all available actions and descriptions
- **Required/optional indicators**: Red underline for missing required attributes
- **Type validation**: Ensures correct data types

Example IntelliSense tooltip for Action attribute:
```
Action (default: "setValue")
The action to perform on the JSON file. Default is 'setValue'.
Choose from: readValue, setValue, deleteValue, replaceJsonValue, 
createJsonPointerValue, appendArray, insertArray, removeArrayElement, 
distinctValues.
```

### Composite Elements

High-level elements with simplified syntax:

#### AppSettings
```xml
<Json:AppSettings 
  Id="SetEnvironment"          <!-- Required: Unique identifier -->
  File="[#AppSettings]"        <!-- Required: Path to JSON file -->
  Key="Environment"            <!-- Required: Dot-notation path -->
  Value="Production"           <!-- Required: Value to set -->
  Sequence="1"                 <!-- Optional: Execution order (default: 1) -->
  CreateIfMissing="yes" />     <!-- Optional: Create if missing (default: yes) -->
```

#### ConnectionString
```xml
<Json:ConnectionString 
  Id="SetConnection"           <!-- Required: Unique identifier -->
  File="[#AppSettings]"        <!-- Required: Path to JSON file -->
  Name="DefaultConnection"     <!-- Required: Connection string name -->
  Value="Server=..."           <!-- Required: Connection string value -->
  Sequence="1" />              <!-- Optional: Execution order (default: 1) -->
```

#### LoggingLevel
```xml
<Json:LoggingLevel 
  Id="SetLogLevel"             <!-- Required: Unique identifier -->
  File="[#AppSettings]"        <!-- Required: Path to JSON file -->
  Category="Default"           <!-- Optional: Log category (default: Default) -->
  Level="Information"          <!-- Required: Log level -->
  Sequence="1" />              <!-- Optional: Execution order (default: 1) -->
```

### JsonTransaction Element

Groups multiple related JSON operations:

```xml
<Json:JsonTransaction 
  Id="DatabaseConfig"          <!-- Required: Unique identifier -->
  File="[#AppSettings]"        <!-- Optional: Default file for nested elements -->
  BaseSequence="10">           <!-- Optional: Base sequence number (default: 1) -->
  
  <!-- Nested JsonFile elements -->
  <Json:JsonFile ... />
  <Json:JsonFile ... />
</Json:JsonTransaction>
```

## IntelliSense Features by Element

### JsonFile
- ✅ Action enumeration with descriptions
- ✅ ElementPath documentation with JSONPath/JSONPointer examples
- ✅ Value attribute contextual help
- ✅ Sequence default value indication
- ✅ OnlyIfExists yes/no enumeration

### AppSettings
- ✅ Key attribute with dot-notation explanation
- ✅ CreateIfMissing yes/no enumeration
- ✅ Automatic JSONPath conversion documentation

### ConnectionString
- ✅ Security warning in Value attribute tooltip
- ✅ Name attribute with examples
- ✅ Automatic ConnectionStrings section documentation

### LoggingLevel
- ✅ Category attribute with common values
- ✅ Level attribute with all .NET log levels
- ✅ Automatic Logging.LogLevel section documentation

### JsonTransaction
- ✅ BaseSequence with auto-sequencing explanation
- ✅ File attribute inheritance documentation
- ✅ Logical grouping benefits

## Tips for Best IntelliSense Experience

1. **Keep namespace declarations at the top:**
   ```xml
   <Wix xmlns="http://wixtoolset.org/schemas/v4/wxs"
        xmlns:Json="http://schemas.hegsie.com/wix/JsonExtension">
   ```

2. **Use Ctrl+Space** to trigger IntelliSense manually in VS Code

3. **Hover over attributes** to read full documentation

4. **Pay attention to red underlines** - they indicate missing required attributes or validation errors

5. **Use Tab to auto-complete** attribute values, especially for Action enumerations

## Troubleshooting

### IntelliSense Not Working

1. **Check namespace declaration**: Ensure `xmlns:Json="http://schemas.hegsie.com/wix/JsonExtension"` is present
2. **Verify NuGet package**: Make sure WixJsonFileExtension package is installed
3. **Restart IDE**: Sometimes a restart is needed after adding the package
4. **Check XML extension**: In VS Code, verify Red Hat XML extension is installed and active
5. **Validate catalog**: Ensure catalog.xml points to the correct schema file path

### Schema Validation Errors

If you see validation errors but your syntax is correct:
1. Ensure you're using WixJsonFileExtension 6.0.0 or later
2. Check that the schema file is accessible at the configured path
3. Clear XML extension cache in VS Code: Command Palette > "XML: Clear cache"

### Missing Attributes

The schema marks required attributes:
- `Id` - Always required for all elements
- `File` - Required for all elements except when inherited from JsonTransaction
- `Action` - Optional, defaults to "setValue"
- Element-specific required attributes are indicated in IntelliSense

## Schema Versioning

The schema namespace is:
```
http://schemas.hegsie.com/wix/JsonExtension
```

This namespace is version-agnostic and will be maintained across versions for backward compatibility. New features will be added without breaking existing elements.

## Further Reading

- [Main README](../README.md) - Complete extension documentation
- [Improved Authoring Experience](../README.md#improved-authoring-experience) - Overview of new features
- [CompositeElements.wxs](../examples/CompositeElements.wxs) - Complete examples
- [WiX v4 XSD Documentation](https://wixtoolset.org/docs/schema/) - WiX schema reference
