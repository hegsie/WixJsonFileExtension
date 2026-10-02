---
title: Update appsettings.json from a WiX installer
description: How to set connection strings, URLs, ports and feature flags in appsettings.json during an MSI install with WiX Toolset v4-v7, using WixJsonFileExtension instead of a custom action.
---

# Update appsettings.json from a WiX installer

.NET applications keep their settings in `appsettings.json`, but WiX only ships `util:XmlFile` for XML. The usual workaround is a C# custom action. That means scheduling it as a deferred action after `InstallFiles`, passing paths in through `CustomActionData`, running it elevated, and writing your own rollback.

[WixJsonFileExtension](https://github.com/hegsie/WixJsonFileExtension) does all of that for you. Each change is one `JsonFile` element.

## 1. Add the package

```xml
<!-- MyInstaller.wixproj -->
<ItemGroup>
  <PackageReference Include="WixJsonFileExtension" Version="7.1.0" />
</ItemGroup>
```

## 2. Edit the file

```xml
<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs"
     xmlns:Json="http://schemas.hegsie.com/wix/JsonExtension">
  <Package Name="MyApp" Manufacturer="Me" Version="1.0.0" UpgradeCode="PUT-GUID-HERE">
    <Property Id="DB_SERVER" Value="localhost" />
    <Property Id="API_URL" Value="https://api.example.com" />
    <Property Id="PORT" Value="5000" />

    <StandardDirectory Id="ProgramFiles64Folder">
      <Directory Id="INSTALLFOLDER" Name="MyApp">
        <Component Id="AppConfig">
          <File Id="AppSettings" Name="appsettings.json" Source="appsettings.json" />

          <!-- A string, with an installer property substituted in -->
          <Json:JsonFile Id="SetConnection" File="[#AppSettings]"
                         ElementPath="$.ConnectionStrings.DefaultConnection"
                         Value="Server=[DB_SERVER];Database=MyApp;Trusted_Connection=True"
                         Action="setValue" />

          <!-- A nested value -->
          <Json:JsonFile Id="SetApiUrl" File="[#AppSettings]"
                         ElementPath="$.Api.BaseUrl" Value="[API_URL]" Action="setValue" />

          <!-- Written as a JSON number, not a string -->
          <Json:JsonFile Id="SetPort" File="[#AppSettings]"
                         ElementPath="$.Kestrel.Port" Value="[PORT]" ValueType="number" />
        </Component>
      </Directory>
    </StandardDirectory>
  </Package>
</Wix>
```

Set the properties on the command line, or from your UI:

```
msiexec /i MyApp.msi DB_SERVER=sql01 API_URL=https://api.contoso.com PORT=8080
```

## How it runs

- Changes are applied after `InstallFiles`, in the deferred, elevated part of the install. Files under Program Files can be written.
- If the install fails or is cancelled, every change is rolled back.
- Key order, indentation and line endings in the file are preserved, so diffs stay small.
- Add `OnlyIfExists="yes"` to change a value only when it is already present.

## Next steps

- [Keep user settings across upgrades](preserve-settings-on-upgrade.md)
- [Cookbook](COOKBOOK.md) for logging, feature flags, environment-specific settings and arrays
- [Full attribute reference](https://github.com/hegsie/WixJsonFileExtension#jsonfile-element-attributes)
