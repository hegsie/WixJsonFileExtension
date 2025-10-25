using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
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
			int attributes = 0;
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

			if (0 != attributes)
			{
				symbol.Flags = attributes;
			}

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
			if (int.TryParse(s, out int i))
			{
				return i;
			}
			return null;
		}
	}

}
