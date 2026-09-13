namespace Hegsie.Wix.JsonExtension
{
	internal enum JsonFlags
	{
		DeleteValue = 1,
		SetValue = 2,
		ReplaceJsonValue = 4,
		CreateJsonPointerValue = 8,
		ReadValue = 16,
		AppendArray = 32,
		InsertArray = 64,
		RemoveArrayElement = 128,
		ValidateSchema = 256,
		DistinctValues = 512,
		OnlyIfExists = 1024,
		// Modifiers (not actions): back the file up before the first change in a transaction, and
		// put that backup back when the component is uninstalled.
		CreateBackup = 2048,
		RestoreOnUninstall = 4096,
		// Formatted="no": the custom action reads Value literally instead of MSI-formatting it.
		RawValue = 8192
	}
}
