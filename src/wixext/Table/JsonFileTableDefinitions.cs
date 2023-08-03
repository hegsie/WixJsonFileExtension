using WixToolset.Data.WindowsInstaller;

namespace Hegsie.Wix.JsonExtension.Table
{
	public static class JsonFileTableDefinitions
	{
		public static readonly TableDefinition JsonFile = new TableDefinition(
			"Wix4JsonFile",
			JsonFileDefinitions.JsonFile,
			new[]
			{
				new ColumnDefinition("JsonConfig", ColumnType.String, 72, primaryKey: true, nullable: false, ColumnCategory.Identifier, description: "Primary key, non-localized token.", modularizeType: ColumnModularizeType.Column),
				new ColumnDefinition("File", ColumnType.String, 0, primaryKey: false, nullable: true, ColumnCategory.Formatted, description: "The .json file in which to write the information", modularizeType: ColumnModularizeType.Property),
				new ColumnDefinition("ElementPath", ColumnType.String, 0, primaryKey: false, nullable: false, ColumnCategory.Formatted, description: "Foreign key, Component used to determine install state", modularizeType: ColumnModularizeType.Property),
				new ColumnDefinition("Value", ColumnType.String, 0, primaryKey: false, nullable: true, ColumnCategory.Formatted, description: "Foreign key, User used to log into database", modularizeType: ColumnModularizeType.Property),
				new ColumnDefinition("Flags", ColumnType.Number, 4, primaryKey: false, nullable: false, ColumnCategory.Unknown,  minValue: 0, maxValue: 65536, description: "deleteValue=1,setValue=2,addArrayValue=4,uninstall=8,preserveDate=16,jsonPointer=32"),
				new ColumnDefinition("Component_", ColumnType.String, 72, primaryKey: false, nullable: false, ColumnCategory.Identifier, keyTable: "Component", keyColumn: 1, description: "Foreign key, Component used to determine install state", modularizeType: ColumnModularizeType.Column),
				new ColumnDefinition("Sequence", ColumnType.Number, 2, primaryKey: false, nullable: true, ColumnCategory.Unknown, description: "Order to execute the JSON file modifications."),
			},
			symbolIdIsPrimaryKey: true
		);

		public static readonly TableDefinition[] All = new[]
		{
			JsonFile,
		};
	}
}
