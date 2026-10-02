---
title: Edit appsettings.json and JSON files from WiX installers
description: WixJsonFileExtension is a WiX Toolset v4-v7 extension that creates, updates, reads and removes values in JSON files such as appsettings.json during MSI install, repair and uninstall.
---

# Edit JSON files from WiX installers

WixJsonFileExtension is a [WiX Toolset](https://wixtoolset.org/) extension for changing JSON configuration files, such as `appsettings.json`, during an MSI install, repair or uninstall. It adds a `JsonFile` element that works like `util:XmlFile` does for XML: you name the file, a JSONPath and a value, and the extension does the deferred, elevated, rolled-back work. There is no custom action code to write.

- WiX Toolset v4, v5, v6 and v7
- x64, x86 and ARM64 installers
- Set, read, delete and replace values; append, insert and remove array items
- Windows Installer properties in values (`[DB_SERVER]`), typed values (number, boolean, JSON)
- Automatic rollback, optional backups, JSON schema validation
- MIT licensed, on [NuGet](https://www.nuget.org/packages/WixJsonFileExtension)

## Quick start

Add the package to your `.wixproj`:

```xml
<PackageReference Include="WixJsonFileExtension" Version="7.1.0" />
```

Declare the namespace and add `JsonFile` elements to the component that installs the file:

```xml
<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs"
     xmlns:Json="http://schemas.hegsie.com/wix/JsonExtension">
  ...
  <Component Id="AppConfig">
    <File Id="AppSettings" Name="appsettings.json" Source="appsettings.json" />

    <Json:JsonFile Id="SetConnection" File="[#AppSettings]"
                   ElementPath="$.ConnectionStrings.DefaultConnection"
                   Value="Server=[DB_SERVER];Database=[DB_NAME];Trusted_Connection=True"
                   Action="setValue" />
  </Component>
```

## Guides

- [Update appsettings.json from a WiX installer](update-appsettings-json.md)
- [Keep user settings in appsettings.json across upgrades](preserve-settings-on-upgrade.md)
- [Cookbook: connection strings, logging, feature flags, API endpoints and more](COOKBOOK.md)
- [IntelliSense for the JsonFile element (XSD schema)](XSD_SCHEMA.md)
- [Full reference (README)](https://github.com/hegsie/WixJsonFileExtension#readme)

## Code signing

Free code signing provided by [SignPath.io](https://about.signpath.io/), certificate by [SignPath Foundation](https://signpath.org/). See the [code signing policy](https://github.com/hegsie/WixJsonFileExtension/blob/main/CODE_SIGNING.md).
