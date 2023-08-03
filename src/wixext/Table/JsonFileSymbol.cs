using WixToolset.Data;

namespace Hegsie.Wix.JsonExtension.Table;

public static partial class JsonFileDefinitions
{
	public static readonly IntermediateSymbolDefinition JsonFile = new IntermediateSymbolDefinition(
		JsonSymbolDefinitionType.JsonFile.ToString(),
		new[]
		{
			new IntermediateFieldDefinition(nameof(JsonFileSymbolFields.File), IntermediateFieldType.String),
			new IntermediateFieldDefinition(nameof(JsonFileSymbolFields.ElementPath), IntermediateFieldType.String),
			new IntermediateFieldDefinition(nameof(JsonFileSymbolFields.Value), IntermediateFieldType.String),
			new IntermediateFieldDefinition(nameof(JsonFileSymbolFields.Flags), IntermediateFieldType.Number),
			new IntermediateFieldDefinition(nameof(JsonFileSymbolFields.ComponentRef), IntermediateFieldType.String),
			new IntermediateFieldDefinition(nameof(JsonFileSymbolFields.Sequence), IntermediateFieldType.Number),
		},
		typeof(JsonFileSymbol));
}

public enum JsonFileSymbolFields
{
	File,
	ElementPath,
	Value,
	Flags,
	ComponentRef,
	Sequence	
}

public class JsonFileSymbol : IntermediateSymbol
{
	public JsonFileSymbol() : base(JsonFileDefinitions.JsonFile, null, null)
	{
	}

	public JsonFileSymbol(SourceLineNumber sourceLineNumber, Identifier id = null) : base(JsonFileDefinitions.JsonFile, sourceLineNumber, id)
	{
	}

	public IntermediateField this[JsonFileSymbolFields index] => this.Fields[(int)index];

	public string File
	{
		get => this.Fields[(int)JsonFileSymbolFields.File].AsString();
		set => this.Set((int)JsonFileSymbolFields.File, value);
	}

	public string ElementPath
	{
		get => this.Fields[(int)JsonFileSymbolFields.ElementPath].AsString();
		set => this.Set((int)JsonFileSymbolFields.ElementPath, value);
	}

	public string Value
	{
		get => this.Fields[(int)JsonFileSymbolFields.Value].AsString();
		set => this.Set((int)JsonFileSymbolFields.Value, value);
	}

	public int Flags
	{
		get => this.Fields[(int)JsonFileSymbolFields.Flags].AsNumber();
		set => this.Set((int)JsonFileSymbolFields.Flags, value);
	}
	public string ComponentRef
	{
		get => this.Fields[(int)JsonFileSymbolFields.ComponentRef].AsString();
		set => this.Set((int)JsonFileSymbolFields.ComponentRef, value);
	}

	public int? Sequence
	{
		get => this.Fields[(int)JsonFileSymbolFields.Sequence].AsNullableNumber();
		set => this.Set((int)JsonFileSymbolFields.Sequence, value);
	}
}
