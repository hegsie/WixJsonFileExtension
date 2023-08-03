using System;
using System.Collections.Generic;
using WixToolset.Extensibility;

namespace Hegsie.Wix.JsonExtension
{
	public class JsonExtensionFactory : BaseExtensionFactory
	{
		protected override IReadOnlyCollection<Type> ExtensionTypes => new[]
		{
			typeof(JsonCompiler),
			typeof(JsonExtensionData),
			typeof(JsonWindowsInstallerBackendBinderExtension),
		};
	}
}
