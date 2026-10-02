---
title: Keep user settings in appsettings.json across WiX upgrades
description: Carry existing values in appsettings.json over a WiX Toolset major upgrade by reading them from the installed file and writing them into the new one with WixJsonFileExtension.
---

# Keep user settings in appsettings.json across upgrades

A major upgrade replaces `appsettings.json` with the new version's copy, so values changed after the first install are lost. To keep particular settings, read them from the installed file before it is replaced, then write them into the new one.

```xml
<Component Id="AppConfig">
  <File Id="AppSettings" Name="appsettings.json" Source="appsettings.json" />

  <!-- readValue runs right after CostFinalize, before the previous version is removed or the
       new file is installed, so it reads the copy already on disk. On a first install the file
       doesn't exist and DefaultValue is used. -->
  <Json:JsonFile Id="ReadApiUrl" File="[INSTALLFOLDER]appsettings.json"
                 ElementPath="$.Api.BaseUrl" Action="readValue"
                 Property="KEEP_API_URL" DefaultValue="https://localhost:5001" />

  <!-- Runs after InstallFiles and writes the value into the new file. -->
  <Json:JsonFile Id="WriteApiUrl" File="[#AppSettings]"
                 ElementPath="$.Api.BaseUrl" Value="[KEEP_API_URL]" Action="setValue" />
</Component>
```

Repeat the pair for each setting you want to keep. This carries over chosen keys; it isn't a general merge:

- Settings you list keep their current values.
- Keys added in the new version get their new defaults.
- Keys removed from the new version stay removed.

Make the `DefaultValue` match the value in the file you ship, so a first install writes the same default back.

## Alternatives

If the application can layer configuration, the cleanest design is often to never edit the shipped file. Keep user overrides in a separate file, for example under `ProgramData`, and add it with `ConfigurationBuilder`. Use the pattern above when the settings have to live in `appsettings.json` itself.

## See also

- [Update appsettings.json from a WiX installer](update-appsettings-json.md)
- [Upgrade, uninstall and rollback behaviour](https://github.com/hegsie/WixJsonFileExtension#readme)
