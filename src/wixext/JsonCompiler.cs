using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text.RegularExpressions;
using System.Xml;
using System.Xml.Linq;
using Hegsie.Wix.JsonExtension.Table;
using WixToolset.Data;
using WixToolset.Extensibility;
using WixToolset.Extensibility.Data;

namespace Hegsie.Wix.JsonExtension
{
	public sealed class JsonCompiler : BaseCompilerExtension
	{
		public override XNamespace Namespace => "http://schemas.hegsie.com/wix/JsonExtension";

		/// <summary>
		/// Processes an element for the Compiler.
		/// </summary>
		/// <param name="sourceLineNumbers">Source line number for the parent element.</param>
		/// <param name="parentElement">Parent element of element to process.</param>
		/// <param name="element">Element to process.</param>
		/// <param name="contextValues">Extra information about the context in which this element is being parsed.</param>
		public override void ParseElement(Intermediate intermediate, IntermediateSection section, XElement parentElement,
			XElement element, IDictionary<string, string> context)
		{
			switch (parentElement.Name.LocalName)
			{
				case "Component":
					string componentId = context["ComponentId"];
					string directoryId = context["DirectoryId"];

					switch (element.Name.LocalName)
					{
						case "JsonFile":
							ParseJsonFileElement(element, componentId, directoryId, section);
							break;
						default:
							ParseHelper.UnexpectedElement(parentElement, element);
							break;
					}
					break;
				default:
					ParseHelper.UnexpectedElement(parentElement, element);
					break;
			}
		}

		/// <summary>
		/// Parses a WixJsonFile element.
		/// </summary>
		/// <param name="node">Element to parse.</param>
		/// <param name="componentId">Identifier of parent component.</param>
		/// <param name="parentDirectory">Identifier of parent component's directory.</param>
		/// <param name="section"></param>
		private void ParseJsonFileElement(XElement node, string componentId, string parentDirectory,
			IntermediateSection section)
		{
			var sourceLineNumbers = ParseHelper.GetSourceLineNumbers(node);
			Identifier id = null;
			string file = null;
			string elementPath = null;
			string value = null;
			string defaultValue = null;
			string property = null;
			int on = CompilerConstants.IntegerNotSet;
			int flags = 0;
			int action = CompilerConstants.IntegerNotSet;
			int? sequence = 1;

			if (node.Attributes().Any())
			{
				foreach (var attribute in node.Attributes())
				{
					if (string.IsNullOrEmpty(attribute.Name.NamespaceName) || Namespace == attribute.Name.Namespace)
					{
						switch (attribute.Name.LocalName)
						{
							case "Id":
								// Identifier for json file modification
								id = ParseHelper.GetAttributeIdentifier(sourceLineNumbers, attribute);
								break;
							case "File":
								// Path of the .json file to modify.
								file = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
								break;
							case "ElementPath":
								// The path to the parent element of the element to be modified. The semantic can be either JSON Path or JSON Pointer language, as specified in the
								// SelectionLanguage attribute. Note that this is a formatted field and therefore, square brackets in the path must be escaped. In addition, JSON Path
								// and Pointer allow backslashes to be used to escape characters, so if you intend to include literal backslashes, you must escape them as well by doubling
								// them in this attribute. The string is formatted by MSI first, and the result is consumed as the JSON Path or Pointer.
								elementPath = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
								break;
							case "Value":
								// The value to set. May be one of the simple JSON types, or a JSON-formatted object. See the
								// <html:a href="http://msdn.microsoft.com/library/aa368609(VS.85).aspx" target="_blank">Formatted topic</html:a> for information how to escape square brackets in the value.
								value = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
								break;
							case "DefaultValue":
								// The value to set. May be one of the simple JSON types, or a JSON-formatted object. See the
								// <html:a href="http://msdn.microsoft.com/library/aa368609(VS.85).aspx" target="_blank">Formatted topic</html:a> for information how to escape square brackets in the value.
								defaultValue = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
								break;
							case "Action":
								// The type of modification to be made to the JSON file when the component is installed or un-installed.
								action = ValidateAction(node, sourceLineNumbers, attribute, ref flags);
								break;
							case "On":
								// Defines when the specified changes to the JSON file are to be done.
								on = ValidateOn(node, sourceLineNumbers, attribute, ref flags);
								break;
							case "Property":
								// The path to the parent element of the element to be modified. The semantic can be either JSON Path or JSON Pointer language, as specified in the
								// SelectionLanguage attribute. Note that this is a formatted field and therefore, square brackets in the path must be escaped. In addition, JSON Path
								// and Pointer allow backslashes to be used to escape characters, so if you intend to include literal backslashes, you must escape them as well by doubling
								// them in this attribute. The string is formatted by MSI first, and the result is consumed as the JSON Path or Pointer.
								property = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
								break;
							case "Sequence":
								// Specifies the order in which the modification is to be attempted on the JSON file.  It is important to ensure that new elements are created before you attempt to modify them.
								sequence = ToNullableInt(ParseHelper.GetAttributeValue(sourceLineNumbers, attribute));
								break;
							default:
								ParseHelper.UnexpectedAttribute(node, attribute);
								break;
						}
					}
					else
					{
						ErrorMessages.UnsupportedExtensionAttribute(sourceLineNumbers, attribute.Parent.Name.ToString(), attribute.Name.ToString());
					}
				}
			}

			if (CompilerConstants.IntegerNotSet == action)
			{
				// default is set value
				flags |= 2;
			}

			foreach (var child in node.Elements())
			{
				if (XmlNodeType.Element != child.NodeType)
				{
					continue;
				}

				if (child.Name.Namespace == Namespace)
				{
					ErrorMessages.UnexpectedElement(sourceLineNumbers, node.Name.ToString(), child.Name.ToString());
				}
				else
				{
					ErrorMessages.UnsupportedExtensionElement(sourceLineNumbers, node.Name.ToString(), child.Name.ToString());
				}
			}

			if (Messaging.EncounteredError)
			{
				return;
			}

			// Validate required attributes and common issues
			ValidateJsonFileElement(node, sourceLineNumbers, action, file, elementPath, value, property);

			if (Messaging.EncounteredError)
			{
				return;
			}

			var symbol = section.AddSymbol(new JsonFileSymbol(sourceLineNumbers, id)
			{
				File = file,
				ElementPath = elementPath,
				Value = value,
				DefaultValue = defaultValue,
				Flags = flags,
				ComponentRef = componentId,
				Sequence = sequence,
				Property = property
			});

			ParseHelper.CreateCustomActionReference(sourceLineNumbers, section,
				action == (int)JsonAction.ReadValue ? "WixPropertyJsonFile" : "WixSchedJsonFile", Context.Platform,
				CustomActionPlatforms.X64);
		}

		private int ValidateSelectionLanguage(XElement node, SourceLineNumber sourceLineNumbers,
			XAttribute attribute, ref int flags)
		{
			int selectionLanguage;
			string selectionLanguageValue = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
			if (selectionLanguageValue.Length == 0)
			{
				selectionLanguage = CompilerConstants.IllegalInteger;
			}
			else
			{
				switch (selectionLanguageValue)
				{
					case "JSONPath":
						selectionLanguage = 1;
						break;
					case "JSONPointer":
						flags |= 32;
						selectionLanguage = 2;
						break;
					default:
						Messaging.Write(ErrorMessages.IllegalAttributeValue(sourceLineNumbers, node.Name.ToString(),
							"SelectionLanguage", selectionLanguageValue, "JSONPath", "JSONPointer"));
						selectionLanguage = CompilerConstants.IllegalInteger;
						break;
				}
			}

			return selectionLanguage;
		}

		private int ValidateOn(XElement node, SourceLineNumber sourceLineNumbers, XAttribute attribute,
			ref int flags)
		{
			int on;
			string onValue = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
			if (onValue.Length == 0)
			{
				on = CompilerConstants.IllegalInteger;
			}
			else
			{
				switch (onValue)
				{
					case "install":
						on = 1;
						break;
					case "uninstall":
						flags |= 8;
						on = 2;
						break;
					case "both":
						on = 3;
						break;
					default:
						Messaging.Write(ErrorMessages.IllegalAttributeValue(sourceLineNumbers, node.Name.ToString(),
							"On", onValue, "install", "uninstall", "both"));
						on = CompilerConstants.IllegalInteger;
						break;
				}
			}

			return on;
		}

		private int ValidateAction(XElement node, SourceLineNumber sourceLineNumbers, XAttribute attribute,
			ref int flags)
		{
			const string ActionDeleteValue = "deleteValue";
			const string ActionSetValue = "setValue";
			const string ActionCreateValue = "createJsonPointerValue";
			const string ActionReplaceJsonValue = "replaceJsonValue";
			const string ActionReadValue = "readValue";

			int action;
			string actionValue = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
			if (actionValue.Length == 0)
			{
				action = CompilerConstants.IllegalInteger;
			}
			else
			{
				switch (actionValue)
				{
					case ActionDeleteValue:
						flags |= (int)JsonFlags.DeleteValue;
						action = (int)JsonAction.DeleteValue;
						break;
					case ActionSetValue:
						flags |= (int)JsonFlags.SetValue;
						action = (int)JsonAction.SetValue;
						break;
					case ActionReplaceJsonValue:
						flags |= (int)JsonFlags.ReplaceJsonValue;
						action = (int)JsonAction.ReplaceJsonValue;
						break;
					case ActionCreateValue:
						flags |= (int)JsonFlags.CreateJsonPointerValue;
						action = (int)JsonAction.CreateJsonPointerValue;
						break;
					case ActionReadValue:
						flags |= (int)JsonFlags.ReadValue;
						action = (int)JsonAction.ReadValue;
						break;
					default:
						Messaging.Write(ErrorMessages.IllegalAttributeValue(sourceLineNumbers, node.Name.ToString(),
							"Action", actionValue, ActionDeleteValue, ActionSetValue, ActionReplaceJsonValue, ActionCreateValue, ActionReadValue));
						action = CompilerConstants.IllegalInteger;
						break;
				}
			}

			return action;
		}

		public int? ToNullableInt(string s)
		{
			if (string.IsNullOrEmpty(s))
			{
				return null;
			}
			if (int.TryParse(s, out int i))
			{
				return i;
			}
			return null;
		}

		/// <summary>
		/// Validates the JsonFile element for common issues and required attributes.
		/// </summary>
		private void ValidateJsonFileElement(XElement node, SourceLineNumber sourceLineNumbers, int action, 
			string file, string elementPath, string value, string property)
		{
			// Validate required attributes based on action
			if (action == (int)JsonAction.ReadValue)
			{
				// readValue requires Property attribute
				if (string.IsNullOrEmpty(property))
				{
					Messaging.Write(ErrorMessages.ExpectedAttribute(sourceLineNumbers, node.Name.ToString(), "Property", "Action", "readValue"));
				}
				// Validate property name format (should be uppercase)
				else if (!string.IsNullOrEmpty(property) && property != property.ToUpperInvariant())
				{
					Messaging.Write(WarningMessages.PropertyNameShouldBeUppercase(sourceLineNumbers, node.Name.ToString(), property));
				}
			}
			else if (action == (int)JsonAction.SetValue || action == (int)JsonAction.ReplaceJsonValue || 
			         action == (int)JsonAction.CreateJsonPointerValue)
			{
				// These actions require Value attribute
				if (string.IsNullOrEmpty(value))
				{
					string actionName = action == (int)JsonAction.SetValue ? "setValue" :
					                   action == (int)JsonAction.ReplaceJsonValue ? "replaceJsonValue" : "createJsonPointerValue";
					Messaging.Write(ErrorMessages.ExpectedAttribute(sourceLineNumbers, node.Name.ToString(), "Value", "Action", actionName));
				}
			}

			// Validate File attribute is present
			if (string.IsNullOrEmpty(file))
			{
				Messaging.Write(ErrorMessages.ExpectedAttribute(sourceLineNumbers, node.Name.ToString(), "File"));
			}

			// Validate ElementPath is present
			if (string.IsNullOrEmpty(elementPath))
			{
				Messaging.Write(ErrorMessages.ExpectedAttribute(sourceLineNumbers, node.Name.ToString(), "ElementPath"));
			}
			else
			{
				// Check for common JSONPath issues
				ValidateElementPath(node, sourceLineNumbers, elementPath, action);
			}

			// Validate property references in Value attribute
			if (!string.IsNullOrEmpty(value))
			{
				ValidatePropertyReferences(node, sourceLineNumbers, value, "Value");
			}

			// Validate property references in File attribute
			if (!string.IsNullOrEmpty(file))
			{
				ValidatePropertyReferences(node, sourceLineNumbers, file, "File");
			}
		}

		/// <summary>
		/// Validates the ElementPath for common issues.
		/// </summary>
		private void ValidateElementPath(XElement node, SourceLineNumber sourceLineNumbers, string elementPath, int action)
		{
			// Check if this is a JSONPointer path (starts with /) for createJsonPointerValue
			bool isJsonPointer = elementPath.StartsWith("/");

			if (action == (int)JsonAction.CreateJsonPointerValue)
			{
				// createJsonPointerValue should use JSONPointer syntax (starts with /)
				if (!isJsonPointer)
				{
					Messaging.Write(WarningMessages.ElementPathShouldStartWithSlash(sourceLineNumbers, node.Name.ToString(), "createJsonPointerValue"));
				}
			}
			else
			{
				// Other actions should use JSONPath syntax (starts with $)
				if (isJsonPointer)
				{
					Messaging.Write(WarningMessages.ElementPathShouldStartWithDollar(sourceLineNumbers, node.Name.ToString()));
				}
				else if (!elementPath.StartsWith("$"))
				{
					Messaging.Write(WarningMessages.ElementPathShouldStartWithDollar(sourceLineNumbers, node.Name.ToString()));
				}

				// Check for unescaped square brackets (common mistake)
				if (ContainsUnescapedBrackets(elementPath))
				{
					Messaging.Write(WarningMessages.UnescapedBracketsInElementPath(sourceLineNumbers, node.Name.ToString(), elementPath));
				}
			}

			// Basic JSONPath syntax validation
			if (!isJsonPointer && elementPath.StartsWith("$"))
			{
				ValidateBasicJsonPathSyntax(node, sourceLineNumbers, elementPath);
			}
		}

		/// <summary>
		/// Checks if the path contains unescaped square brackets (MSI formatting issue).
		/// </summary>
		private bool ContainsUnescapedBrackets(string path)
		{
			// Look for patterns like [0] or [*] that are not escaped as [\[]0[\]]
			// This is a simplified heuristic check that looks for likely JSONPath bracket expressions
			for (int i = 0; i < path.Length; i++)
			{
				if (path[i] == '[')
				{
					// Check if this is part of an escape sequence like [\[] or [\]]
					if (i + 3 < path.Length)
					{
						string sequence = path.Substring(i, 4);
						if (sequence == "[\\[]" || sequence == "[\\]]")
						{
							i += 3; // Skip the escape sequence
							continue;
						}
					}
					// Check if this is a property reference or JSONPath expression
					int closeIndex = path.IndexOf(']', i + 1);
					if (closeIndex > i)
					{
						string content = path.Substring(i + 1, closeIndex - i - 1);
						// JSONPath array indices are numeric, wildcards are *, or filter expressions start with ?
						// MSI properties are typically alphabetic or start with special chars like # ! $ %
						if (Regex.IsMatch(content, @"^\d+$") || content == "*" || 
						    (content.StartsWith("?") && content.Contains("@")))
						{
							return true; // Likely unescaped JSONPath bracket
						}
					}
				}
			}
			return false;
		}

		/// <summary>
		/// Performs basic JSONPath syntax validation.
		/// </summary>
		private void ValidateBasicJsonPathSyntax(XElement node, SourceLineNumber sourceLineNumbers, string jsonPath)
		{
			// Check for common syntax errors
			// This is not a complete JSONPath parser, just catches common mistakes

			// Check for double dots without following valid path segment  
			// Recursive descent can be followed by property name, bracket, or wildcard
			// Use a more permissive check to avoid false positives
			if (jsonPath.Contains(".."))
			{
				int dotsIndex = jsonPath.IndexOf("..");
				if (dotsIndex + 2 < jsonPath.Length)
				{
					char nextChar = jsonPath[dotsIndex + 2];
					// Check if followed by something that looks wrong (not a letter, [, *, or $)
					if (nextChar != '.' && nextChar != '[' && nextChar != '*' && nextChar != '$' && !char.IsLetter(nextChar))
					{
						Messaging.Write(WarningMessages.InvalidJsonPathSyntax(sourceLineNumbers, node.Name.ToString(), 
							jsonPath, "Recursive descent (..) should be followed by a property name, bracket, or wildcard"));
					}
				}
			}

			// Check for invalid characters after $
			if (jsonPath.StartsWith("$") && jsonPath.Length > 1 && jsonPath[1] != '.' && jsonPath[1] != '[')
			{
				Messaging.Write(WarningMessages.InvalidJsonPathSyntax(sourceLineNumbers, node.Name.ToString(), 
					jsonPath, "JSONPath should start with $ followed by . or ["));
			}

			// Check for unclosed brackets (even if escaped)
			int openBrackets = 0;
			bool inEscape = false;
			foreach (char c in jsonPath)
			{
				if (c == '\\')
				{
					inEscape = !inEscape;
					continue;
				}
				if (!inEscape)
				{
					if (c == '[') openBrackets++;
					if (c == ']') openBrackets--;
				}
				inEscape = false;
			}
			if (openBrackets != 0)
			{
				Messaging.Write(WarningMessages.UnmatchedBracketsInElementPath(sourceLineNumbers, node.Name.ToString()));
			}
		}

		/// <summary>
		/// Validates property references in attribute values.
		/// </summary>
		private void ValidatePropertyReferences(XElement node, SourceLineNumber sourceLineNumbers, string attributeValue, string attributeName)
		{
			// Find all property references [PROPERTY_NAME]
			var propertyPattern = new Regex(@"\[([^\]]+)\]");
			var matches = propertyPattern.Matches(attributeValue);

			foreach (Match match in matches)
			{
				string propertyRef = match.Groups[1].Value;
				
				// Skip special references like [#FileId], [!ComponentId], [$DirectoryId], etc.
				if (propertyRef.StartsWith("#") || propertyRef.StartsWith("!") || 
				    propertyRef.StartsWith("$") || propertyRef.StartsWith("%"))
				{
					continue;
				}

				// Skip escaped brackets [\[] and [\]]
				if (propertyRef == "\\[" || propertyRef == "\\]")
				{
					continue;
				}

				// Check if property name is uppercase (MSI convention)
				if (propertyRef != propertyRef.ToUpperInvariant())
				{
					Messaging.Write(WarningMessages.PropertyReferenceShouldBeUppercase(sourceLineNumbers, 
						node.Name.ToString(), attributeName, propertyRef));
				}
			}
		}
	}

}
