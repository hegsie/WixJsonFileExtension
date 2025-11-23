using System;
using WixToolset.Data;

namespace Hegsie.Wix.JsonExtension
{
	/// <summary>
	/// Custom warning messages for JSON extension.
	/// </summary>
	internal static class WarningMessages
	{
		public static Message PropertyNameShouldBeUppercase(SourceLineNumber sourceLineNumbers, string elementName, string propertyName)
		{
			return Message(sourceLineNumbers, Ids.PropertyNameShouldBeUppercase, 
				"The Property attribute '{1}' in {0} element should be uppercase according to Windows Installer conventions. Consider using '{2}' instead.",
				elementName, propertyName, propertyName.ToUpperInvariant());
		}

		public static Message PropertyReferenceShouldBeUppercase(SourceLineNumber sourceLineNumbers, string elementName, string attributeName, string propertyRef)
		{
			return Message(sourceLineNumbers, Ids.PropertyReferenceShouldBeUppercase,
				"The property reference '[{2}]' in {0} element's {1} attribute should be uppercase. Consider using '[{3}]' instead.",
				elementName, attributeName, propertyRef, propertyRef.ToUpperInvariant());
		}

		public static Message UnescapedBracketsInElementPath(SourceLineNumber sourceLineNumbers, string elementName, string elementPath)
		{
			return Message(sourceLineNumbers, Ids.UnescapedBracketsInElementPath,
				@"The ElementPath attribute in {0} element may contain unescaped square brackets. Square brackets must be escaped as [\[] and [\]] for MSI property formatting. Path: {1}",
				elementName, elementPath);
		}

		public static Message ElementPathShouldStartWithDollar(SourceLineNumber sourceLineNumbers, string elementName)
		{
			return Message(sourceLineNumbers, Ids.ElementPathShouldStartWithDollar,
				"The ElementPath attribute in {0} element should start with '$' for JSONPath syntax (e.g., '$.propertyName').",
				elementName);
		}

		public static Message ElementPathShouldStartWithSlash(SourceLineNumber sourceLineNumbers, string elementName, string actionName)
		{
			return Message(sourceLineNumbers, Ids.ElementPathShouldStartWithSlash,
				"The ElementPath attribute in {0} element should start with '/' for JSONPointer syntax when using Action='{1}' (e.g., '/propertyName').",
				elementName, actionName);
		}

		public static Message InvalidJsonPathSyntax(SourceLineNumber sourceLineNumbers, string elementName, string jsonPath, string reason)
		{
			return Message(sourceLineNumbers, Ids.InvalidJsonPathSyntax,
				"The ElementPath attribute in {0} element may have invalid JSONPath syntax. Path: {1}. Reason: {2}",
				elementName, jsonPath, reason);
		}

		public static Message UnmatchedBracketsInElementPath(SourceLineNumber sourceLineNumbers, string elementName)
		{
			return Message(sourceLineNumbers, Ids.UnmatchedBracketsInElementPath,
				"The ElementPath attribute in {0} element has unmatched brackets. Ensure all '[' have a corresponding ']'.",
				elementName);
		}

		private static Message Message(SourceLineNumber sourceLineNumber, Ids id, string format, params object[] args)
		{
			return new Message(sourceLineNumber, MessageLevel.Warning, (int)id, format, args);
		}

		public enum Ids
		{
			PropertyNameShouldBeUppercase = 7000,
			PropertyReferenceShouldBeUppercase = 7001,
			UnescapedBracketsInElementPath = 7002,
			ElementPathShouldStartWithDollar = 7003,
			ElementPathShouldStartWithSlash = 7004,
			InvalidJsonPathSyntax = 7005,
			UnmatchedBracketsInElementPath = 7006,
		}
	}
}
