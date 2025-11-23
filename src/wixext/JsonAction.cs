namespace Hegsie.Wix.JsonExtension
{
	internal enum JsonAction
	{
		DeleteValue = 1,
		SetValue = 2,
		ReplaceJsonValue = 4,
		CreateJsonPointerValue = 8,
		ReadValue = 16,
		AppendArray = 32,
		InsertArray = 64,
		RemoveArrayElement = 128
	}
}
