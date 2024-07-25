using Hegsie.Wix.JsonExtension.Table;
using WixToolset.Data;
using WixToolset.Extensibility;

namespace Hegsie.Wix.JsonExtension
{
	/// <summary>
	/// The WiX Toolset SQL Server Extension.
	/// </summary>
	public sealed class JsonExtensionData : BaseExtensionData
	{
		public override bool TryGetSymbolDefinitionByName(string name, out IntermediateSymbolDefinition symbolDefinition)
		{
			symbolDefinition = JsonFileDefinitions.ByName(name);
			return symbolDefinition != null;
		}

		public override Intermediate GetLibrary(ISymbolDefinitionCreator symbolDefinitions)
		{
			return Intermediate.Load(typeof(JsonExtensionData).Assembly, "Hegsie.Wix.JsonExtension.Data.json.wixlib", symbolDefinitions);

		}
	}
}
