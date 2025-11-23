using WixToolset.Data.WindowsInstaller;

namespace Hegsie.Wix.JsonExtension.Table
{
	public static class JsonFileTableDefinitions
	{
		public static readonly TableDefinition JsonFile = new TableDefinition(
			"WixJsonFile",
			JsonFileDefinitions.JsonFile,
			new[]
			{
				new ColumnDefinition("JsonConfig", ColumnType.String, 72, primaryKey: true, nullable: false, ColumnCategory.Identifier, description: "Primary key, non-localized token.", modularizeType: ColumnModularizeType.Column),
				new ColumnDefinition("File", ColumnType.String, 0, primaryKey: false, nullable: true, ColumnCategory.Formatted, description: "The .json file in which to write the information", modularizeType: ColumnModularizeType.Property),
				new ColumnDefinition("ElementPath", ColumnType.String, 0, primaryKey: false, nullable: false, ColumnCategory.Formatted, description: "ElementPath used as target for manipulating json file", modularizeType: ColumnModularizeType.Property),
				new ColumnDefinition("Value", ColumnType.String, 0, primaryKey: false, nullable: true, ColumnCategory.Formatted, description: "Value used to set into the specified json file", modularizeType: ColumnModularizeType.Property),
				new ColumnDefinition("DefaultValue", ColumnType.String, 0, primaryKey: false, nullable: true, ColumnCategory.Formatted, description: "Default Value used to load into a property in a readValue action when unable to locate json value", modularizeType: ColumnModularizeType.Property),
				new ColumnDefinition("Flags", ColumnType.Number, 4, primaryKey: false, nullable: false, ColumnCategory.Unknown,  minValue: 0, maxValue: 65536, description: "deleteValue=1,setValue=2,replaceJsonValue=4,createJsonPointerValue=8,readValue=16,appendArray=32,insertArray=64,removeArrayElement=128,validateSchema=256"),
				new ColumnDefinition("Component_", ColumnType.String, 72, primaryKey: false, nullable: false, ColumnCategory.Identifier, keyTable: "Component", keyColumn: 1, description: "Foreign key, Component used to determine install state", modularizeType: ColumnModularizeType.Column),
				new ColumnDefinition("Sequence", ColumnType.Number, 2, primaryKey: false, nullable: true, ColumnCategory.Unknown, description: "Order to execute the JSON file modifications."),
				new ColumnDefinition("Property", ColumnType.String, 0, primaryKey: false, nullable: true, ColumnCategory.Unknown, description: "Property to load the json value into when executing a readValue action"),
				new ColumnDefinition("Index", ColumnType.Number, 4, primaryKey: false, nullable: true, ColumnCategory.Unknown, description: "Index for array insert operations. -1 or omitted means append to end."),
				new ColumnDefinition("SchemaFile", ColumnType.String, 0, primaryKey: false, nullable: true, ColumnCategory.Formatted, description: "Path to JSON schema file for validation", modularizeType: ColumnModularizeType.Property),
			},
			symbolIdIsPrimaryKey: true
		);

		public static readonly TableDefinition[] All = new[]
		{
			JsonFile,
		};
	}
}
