using System.Collections.Generic;
using Hegsie.Wix.JsonExtension.Table;
using WixToolset.Data.WindowsInstaller;
using WixToolset.Extensibility;

namespace Hegsie.Wix.JsonExtension
{
	public class JsonWindowsInstallerBackendBinderExtension : BaseWindowsInstallerBackendBinderExtension
	{
		public override IReadOnlyCollection<TableDefinition> TableDefinitions => JsonFileTableDefinitions.All;
	}
}
