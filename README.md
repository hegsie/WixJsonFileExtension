# WixJsonFileExtension (4+)
#### An extension to Windows&reg; installer XML (WiX) to create or modify JSON-formatted files during an installation.

## Status: JSONPath supported for install only applications

To Use this Wix Extension
1. Add a package reference to your project with `dotnet add package WixJsonFileExtension`
2. Add a schema reference inside your wxs file to 
```xml
xmlns:Json="http://schemas.hegsie.com/wix/JsonExtension"
```
3. Use the JsonFile XML element inside your wxs to update your target jaon file: 
```xml
<Json:JsonFile Id="appSettingsSetBooks" File="[#JsonConfig]" ElementPath="$.store.book" Value="[MY_BOOKS]" Action="addArrayValue" />
```

Possible Actions include
 - deleteValue
 - setValue (this is the default if one isn't specified)
 - addArrayValue
 - createJsonPointerValue

 All actions use JSONPaths in their ElementPaths unless specified in their action i.e. createJsonPointerValue

3. Refer to the product.wxs inside the TestJsonConfigInstaller project here for examples on how to manipulate your JSON file inside of an MSI

NB: There are a lot of complex scenarios when using JSONPath (like multi-select inside arrays etc, which don't currently work) 
    try to keep use cases simple where possible or report the issue here and I'll try and see if it can be implemented.

[Windows&reg; installer XML](http://wixtoolset.org/) is an open-source set of tools to create Windows® software installation setups (\*.msi), using XML files to define the content and behavior of the setup.
One important step in most setups is to modify configuration files to reflect either settings specified during the setup by the user, or other settings specific to the individual installation.
Windows&reg; installer has built-in actions to modify the classic ini-files (\*.ini), and WiX provides extensions (and the required custom actions) to modify XML files, and in particular the Application Configuration files (\*.exe.config or \*.dll.config) used by Microsoft&reg; .NET applications.
Today, a third format has become popular, especially in web applications: [JSON](https://www.json.org/) - JavaScript Object Notation.
Often used in networking scenarios (REST services, JQuery, etc.), it is also used as a local data store format, including configuration files.
Especially [.NET Core](https://github.com/dotnet/core) relies heavily on JSON-formatted configuration files, both during development and runtime.

This extension for WiX provides methods to modify JSON files (\*.json) during a software installation.
The new XML elements provided by this extension - JsonFile, work very similar to the existing extensions for XML files, [XmlFile](http://wixtoolset.org/documentation/manual/v3/xsd/util/xmlfile.html)

## Acknowledgements
WixJsonFileExtension uses [jsoncons](https://github.com/danielaparker/jsoncons) by Daniel Parker to read and manipulate JSON files.
