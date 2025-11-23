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
		OnlyIfExists = 1024
	}
}
