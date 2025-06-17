using System;
using System.Collections.Generic;
using System.Text;

namespace Hegsie.Wix.JsonExtension.Table
{
	internal enum JsonAction
	{
		DeleteValue = 1,
		SetValue,
		ReplaceJsonValue,
		CreateJsonPointerValue,
		ReadValue,
	}
}
