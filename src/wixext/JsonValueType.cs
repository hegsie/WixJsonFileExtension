namespace Hegsie.Wix.JsonExtension
{
	/// <summary>
	/// How the Value attribute is converted to a JSON value (JsonFile/@ValueType), stored in the
	/// WixJsonFile.ValueType column. Auto is the historical behaviour: JSON when the text parses as
	/// JSON, otherwise a string, and an existing string stays a string.
	/// </summary>
	internal enum JsonValueType
	{
		Auto = 0,
		String = 1,
		Number = 2,
		Boolean = 3,
		Null = 4,
		Json = 5,
		Date = 6
	}
}
