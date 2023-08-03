using System;

namespace Hegsie.Wix.JsonExtension
{
	[Flags]
	internal enum TableFlags
	{
		DeleteValue = 1,
		SetValue = 2,
		AddArrayValue = 4,
		Uninstall = 8,
		PreserveDate = 16,
		JsonPointer = 32
	}
}
