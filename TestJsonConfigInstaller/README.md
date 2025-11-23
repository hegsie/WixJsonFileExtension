# Test Json Config Installer

This directory contains test and example files for the WixJsonFileExtension.

## Files

### Built Installer
- **Product.wxs** - Main WiX source file that builds the test MSI installer
- **TestJsonConfigInstaller.wixproj** - WiX project file for building the installer
- **appsettings.json** - Test JSON configuration file used in the installer

### Example Files (Not Built)
These files are examples demonstrating common .NET configuration patterns. They are NOT included in the build:

- **DotNetPatterns.wxs** - Standalone example showing .NET configuration patterns (connection strings, logging, etc.)
- **appsettings.dotnet.json** - Example JSON file with typical .NET app structure
- **invalid.json** - Example of intentionally malformed JSON for testing error handling
- **appsettings-schema.json** - JSON schema example
- **config.template.json** - Configuration template example
- **empty.json** - Empty JSON file example

These example files are used for documentation and CI testing purposes but are not compiled into the installer.
