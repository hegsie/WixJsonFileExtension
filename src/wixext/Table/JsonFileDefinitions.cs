using System;
using WixToolset.Data;

namespace Hegsie.Wix.JsonExtension.Table
{
	public enum JsonSymbolDefinitionType
    {
        JsonFile,
    }

    public static partial class JsonFileDefinitions
    {
        public static readonly Version Version = new Version("5.0.0");

        public static IntermediateSymbolDefinition ByName(string name)
        {
            if (!Enum.TryParse(name, out JsonSymbolDefinitionType type))
            {
                return null;
            }

            return ByType(type);
        }

        public static IntermediateSymbolDefinition ByType(JsonSymbolDefinitionType type)
        {
            switch (type)
            {
	            case JsonSymbolDefinitionType.JsonFile:
                    return JsonFileDefinitions.JsonFile;
					
                default:
                    throw new ArgumentOutOfRangeException(nameof(type));
            }
        }
    }
}
