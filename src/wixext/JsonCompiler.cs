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
		// Compiled regex patterns for performance
		private static readonly Regex NumericIndexPattern = new Regex(@"^\d+$", RegexOptions.Compiled);
		private static readonly Regex PropertyReferencePattern = new Regex(@"\[([^\]]+)\]", RegexOptions.Compiled);

		// Elements without an explicit Sequence get an increasing default so operations on the
		// same file execute in authoring order instead of tying at Sequence=1 (which leaves the
		// execution order undefined - the custom action sorts by File, Sequence).
		private int _nextDefaultSequence = 1;

		/// <summary>
		/// Resolves the sequence for an element: the authored value wins, then a transaction
		/// override, then the next default. Future defaults always start above any sequence
		/// handed out here so unsequenced elements never tie with earlier ones.
		/// </summary>
		private int ResolveSequence(int? authoredSequence, int? sequenceOverride = null)
		{
			int sequence = authoredSequence ?? sequenceOverride ?? _nextDefaultSequence;
			if (sequence >= _nextDefaultSequence)
			{
				_nextDefaultSequence = sequence + 1;
			}
			return sequence;
		}

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
							ParseJsonFileElement(intermediate, element, componentId, directoryId, section);
							break;
						case "JsonTransaction":
							ParseJsonTransactionElement(intermediate, element, componentId, directoryId, section);
							break;
						case "AppSettings":
							ParseAppSettingsElement(element, componentId, directoryId, section);
							break;
						case "ConnectionString":
							ParseConnectionStringElement(element, componentId, directoryId, section);
							break;
						case "LoggingLevel":
							ParseLoggingLevelElement(element, componentId, directoryId, section);
							break;
						default:
							ParseHelper.UnexpectedElement(parentElement, element);
							break;
					}
					break;
				case "JsonTransaction":
					if (element.Name.LocalName == "JsonFile")
					{
						string componentId2 = context["ComponentId"];
						string directoryId2 = context["DirectoryId"];
						ParseJsonFileElement(intermediate, element, componentId2, directoryId2, section);
					}
					else
					{
						ParseHelper.UnexpectedElement(parentElement, element);
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
		/// <param name="intermediate">Parent intermediate, needed to hand foreign-namespace
		/// attributes and elements to the extension that owns them.</param>
		/// <param name="node">Element to parse.</param>
		/// <param name="componentId">Identifier of parent component.</param>
		/// <param name="parentDirectory">Identifier of parent component's directory.</param>
		/// <param name="section"></param>
		private void ParseJsonFileElement(Intermediate intermediate, XElement node, string componentId, string parentDirectory,
			IntermediateSection section, string fileOverride = null, int? sequenceOverride = null)
		{
			var sourceLineNumbers = ParseHelper.GetSourceLineNumbers(node);
			Identifier id = null;
			string file = null;
			string elementPath = null;
			string value = null;
			string defaultValue = null;
			string property = null;
			string schemaFile = null;
			int flags = 0;
			int action = CompilerConstants.IntegerNotSet;
			int? sequence = null;
			int? index = null;
			JsonTiming on = JsonTiming.Install;
			string backupSuffix = null;
			bool createBackup = false;
			bool restoreOnUninstall = false;
			JsonValueType valueType = JsonValueType.Auto;
			string culture = null;
			bool formatted = true;

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
								// The path to the element to be modified. The syntax (JSON Path or JSON Pointer) is implied by the Action:
								// createJsonPointerValue consumes a JSON Pointer, every other action consumes a JSON Path. Note that this is a
								// formatted field and therefore square brackets in the path must be escaped. In addition, JSON Path and Pointer
								// allow backslashes to escape characters, so literal backslashes must be doubled. The string is formatted by MSI
								// first, and the result is consumed as the JSON Path or Pointer.
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
								// The type of modification to be made to the JSON file.
								action = ValidateAction(node, sourceLineNumbers, attribute, ref flags);
								break;
							case "On":
								// When the modification runs: while the component is being installed (default), while it
								// is being uninstalled, or both. The same Action and Value apply at each time; a revert is
								// authored as a separate JsonFile element with On="uninstall".
								string onValue = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
								if (!TryParseOn(onValue, out on))
								{
									Messaging.Write(ErrorMessages.IllegalAttributeValue(sourceLineNumbers, node.Name.ToString(),
										"On", onValue, OnInstall, OnUninstall, OnBoth));
								}
								break;
							case "Property":
								// The Windows Installer property that receives the value read from the JSON file. Used with the readValue action.
								property = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
								break;
							case "Sequence":
								// Specifies the order in which the modification is to be attempted on the JSON file.  It is important to ensure that new elements are created before you attempt to modify them.
								sequence = ToNullableInt(ParseHelper.GetAttributeValue(sourceLineNumbers, attribute));
								break;
							case "Index":
								// For array operations, specifies the index at which to insert or remove an element. For insertArray, -1 means append to the end.
								index = ToNullableInt(ParseHelper.GetAttributeValue(sourceLineNumbers, attribute));
								break;
							case "SchemaFile":
								// Optional path to a JSON schema file for validation. The JSON file will be validated against this schema after modifications.
								schemaFile = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
								if (!string.IsNullOrEmpty(schemaFile))
								{
									flags |= (int)JsonFlags.ValidateSchema;
								}
								break;
							case "Formatted":
								// Whether Value goes through MSI's Formatted processing (property and environment
								// variable expansion, [\[] escapes). "no" takes it literally, which is the only way
								// to author a value containing square brackets, such as a JSON array.
								formatted = !ParseYesNo(node, sourceLineNumbers, attribute, "Formatted", defaultYes: true) ? false : true;
								break;
							case "ValueType":
								// How Value is converted to JSON: auto (default), string, number, boolean, null, json, date.
								string valueTypeText = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
								if (!TryParseValueType(valueTypeText, out valueType))
								{
									Messaging.Write(ErrorMessages.IllegalAttributeValue(sourceLineNumbers, node.Name.ToString(),
										"ValueType", valueTypeText, "auto", "string", "number", "boolean", "null", "json", "date"));
								}
								break;
							case "Culture":
								// Culture whose conventions ValueType="number" (decimal and grouping separators) and
								// ValueType="date" (date order, separators) are parsed with, e.g. "de-DE". Formatted,
								// so it may come from a property.
								culture = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
								break;
							case "CreateBackup":
								// Copy the file to <File><BackupSuffix> before the first modification in a transaction,
								// unless that backup already exists, so an administrator can always get the original back.
								createBackup = ParseYesNo(node, sourceLineNumbers, attribute, "CreateBackup");
								break;
							case "BackupSuffix":
								// Suffix of the backup file. Requires CreateBackup="yes"; defaults to ".wixbak".
								backupSuffix = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
								break;
							case "RestoreOnUninstall":
								// When the component is uninstalled, copy the backup back over the file (and remove the
								// backup) before any uninstall-time modifications run. Requires CreateBackup="yes".
								restoreOnUninstall = ParseYesNo(node, sourceLineNumbers, attribute, "RestoreOnUninstall");
								break;
							case "OnlyIfExists":
								// Optional attribute to only perform the action if the element path already exists
								string onlyIfExistsValue = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
								if (!string.IsNullOrEmpty(onlyIfExistsValue))
								{
									if (onlyIfExistsValue.Equals("yes", System.StringComparison.OrdinalIgnoreCase))
									{
										flags |= (int)JsonFlags.OnlyIfExists;
									}
									else if (!onlyIfExistsValue.Equals("no", System.StringComparison.OrdinalIgnoreCase))
									{
										Messaging.Write(ErrorMessages.IllegalAttributeValue(sourceLineNumbers, node.Name.ToString(),
											"OnlyIfExists", onlyIfExistsValue, "yes", "no"));
									}
								}
								break;
							default:
								ParseHelper.UnexpectedAttribute(node, attribute);
								break;
						}
					}
					else
					{
						// Attribute from another extension's namespace: let that extension parse it.
						// The core reports the "unhandled extension attribute" error when no loaded
						// extension owns the namespace.
						ParseHelper.ParseExtensionAttribute(Context.Extensions, intermediate, section, node, attribute);
					}
				}
			}

			if (CompilerConstants.IntegerNotSet == action)
			{
				// default is set value
				action = (int)JsonAction.SetValue;
				flags |= (int)JsonFlags.SetValue;
			}

			if (!formatted)
			{
				// The Value column is validated by ICE03 as a Formatted string, so a literal value is
				// stored with MSI's bracket escapes ([\[] and [\]]), which is valid Formatted syntax. The
				// custom action reads raw values without formatting and undoes exactly this escaping.
				flags |= (int)JsonFlags.RawValue;
				value = EscapeBracketsForRawValue(value);
			}

			if (!string.IsNullOrEmpty(culture) && valueType != JsonValueType.Number && valueType != JsonValueType.Date)
			{
				Messaging.Write(ErrorMessages.IllegalAttributeWithoutOtherAttributes(sourceLineNumbers, node.Name.ToString(), "Culture", "ValueType"));
			}

			if (valueType != JsonValueType.Auto)
			{
				bool takesValue = action == (int)JsonAction.SetValue || action == (int)JsonAction.CreateJsonPointerValue ||
				                  action == (int)JsonAction.AppendArray || action == (int)JsonAction.InsertArray ||
				                  action == (int)JsonAction.RemoveArrayElement;
				if (!takesValue)
				{
					Messaging.Write(ErrorMessages.IllegalAttributeWithOtherAttribute(sourceLineNumbers, node.Name.ToString(), "ValueType", "Action"));
				}
			}

			if (createBackup)
			{
				flags |= (int)JsonFlags.CreateBackup;
				if (string.IsNullOrEmpty(backupSuffix))
				{
					backupSuffix = ".wixbak";
				}
				if (restoreOnUninstall)
				{
					flags |= (int)JsonFlags.RestoreOnUninstall;
				}
			}
			else
			{
				if (!string.IsNullOrEmpty(backupSuffix))
				{
					Messaging.Write(ErrorMessages.IllegalAttributeWithoutOtherAttributes(sourceLineNumbers, node.Name.ToString(), "BackupSuffix", "CreateBackup"));
				}
				if (restoreOnUninstall)
				{
					Messaging.Write(ErrorMessages.IllegalAttributeWithoutOtherAttributes(sourceLineNumbers, node.Name.ToString(), "RestoreOnUninstall", "CreateBackup"));
				}
				backupSuffix = null;
			}

			// Apply values inherited from a parent JsonTransaction, then fall back to a
			// deterministic default sequence when none was authored.
			if (string.IsNullOrEmpty(file) && !string.IsNullOrEmpty(fileOverride))
			{
				file = fileOverride;
			}

			sequence = ResolveSequence(sequence, sequenceOverride);

			foreach (var child in node.Elements())
			{
				if (XmlNodeType.Element != child.NodeType)
				{
					continue;
				}

				if (child.Name.Namespace == Namespace)
				{
					Messaging.Write(ErrorMessages.UnexpectedElement(sourceLineNumbers, node.Name.ToString(), child.Name.ToString()));
				}
				else
				{
					// Child element from another extension's namespace: let that extension parse it.
					ParseHelper.ParseExtensionElement(Context.Extensions, intermediate, section, node, child);
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
				Property = property,
				Index = index,
				SchemaFile = schemaFile,
				On = (int)on,
				BackupSuffix = backupSuffix,
				ValueType = valueType == JsonValueType.Auto ? (int?)null : (int)valueType,
				Culture = string.IsNullOrEmpty(culture) ? null : culture
			});

			// Each timing has its own immediate scheduling action because the two phases sit at
			// different points of the InstallExecuteSequence: install-time work runs after
			// InstallFiles, uninstall-time work must run before RemoveFiles while the target file
			// still exists. readValue is a single immediate action (after CostFinalize) that runs in
			// both phases and gates on the timing itself.
			const CustomActionPlatforms allPlatforms = CustomActionPlatforms.X86 | CustomActionPlatforms.X64 | CustomActionPlatforms.ARM64;
			if (action == (int)JsonAction.ReadValue)
			{
				ParseHelper.CreateCustomActionReference(sourceLineNumbers, section, "WixPropertyJsonFile", Context.Platform, allPlatforms);
			}
			else
			{
				if (on.HasFlag(JsonTiming.Install))
				{
					ParseHelper.CreateCustomActionReference(sourceLineNumbers, section, "WixSchedJsonFile", Context.Platform, allPlatforms);
				}
				// The restore is itself an uninstall-time operation, whatever the row's own timing.
				if (on.HasFlag(JsonTiming.Uninstall) || restoreOnUninstall)
				{
					ParseHelper.CreateCustomActionReference(sourceLineNumbers, section, "WixSchedJsonFileUninstall", Context.Platform, allPlatforms);
				}
			}
		}

		/// <summary>
		/// Parses a yes/no attribute, reporting anything else as an illegal value (which reads as the default).
		/// </summary>
		private bool ParseYesNo(XElement node, SourceLineNumber sourceLineNumbers, XAttribute attribute, string attributeName, bool defaultYes = false)
		{
			string value = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
			if (value.Equals("yes", System.StringComparison.OrdinalIgnoreCase))
			{
				return true;
			}
			if (value.Equals("no", System.StringComparison.OrdinalIgnoreCase))
			{
				return false;
			}
			if (!string.IsNullOrEmpty(value))
			{
				Messaging.Write(ErrorMessages.IllegalAttributeValue(sourceLineNumbers, node.Name.ToString(), attributeName, value, "yes", "no"));
			}
			return defaultYes;
		}

		/// <summary>
		/// Escapes square brackets the way Windows Installer's Formatted syntax does, so a literal
		/// value passes ICE03. Reversed by the custom action for Formatted="no" values.
		/// </summary>
		internal static string EscapeBracketsForRawValue(string value)
		{
			if (string.IsNullOrEmpty(value))
			{
				return value;
			}
			var sb = new System.Text.StringBuilder(value.Length + 8);
			foreach (char c in value)
			{
				if (c == '[') sb.Append("[\\[]");
				else if (c == ']') sb.Append("[\\]]");
				else sb.Append(c);
			}
			return sb.ToString();
		}

		/// <summary>
		/// Parses the ValueType attribute value. An empty value means auto.
		/// </summary>
		internal static bool TryParseValueType(string value, out JsonValueType valueType)
		{
			switch (value)
			{
				case null:
				case "":
				case "auto": valueType = JsonValueType.Auto; return true;
				case "string": valueType = JsonValueType.String; return true;
				case "number": valueType = JsonValueType.Number; return true;
				case "boolean": valueType = JsonValueType.Boolean; return true;
				case "null": valueType = JsonValueType.Null; return true;
				case "json": valueType = JsonValueType.Json; return true;
				case "date": valueType = JsonValueType.Date; return true;
				default: valueType = JsonValueType.Auto; return false;
			}
		}

		private const string OnInstall = "install";
		private const string OnUninstall = "uninstall";
		private const string OnBoth = "both";

		/// <summary>
		/// Parses the On attribute value. An empty value means the default (install).
		/// </summary>
		internal static bool TryParseOn(string value, out JsonTiming timing)
		{
			switch (value)
			{
				case null:
				case "":
				case OnInstall:
					timing = JsonTiming.Install;
					return true;
				case OnUninstall:
					timing = JsonTiming.Uninstall;
					return true;
				case OnBoth:
					timing = JsonTiming.Both;
					return true;
				default:
					timing = JsonTiming.Install;
					return false;
			}
		}

		private int ValidateAction(XElement node, SourceLineNumber sourceLineNumbers, XAttribute attribute,
			ref int flags)
		{
			const string ActionDeleteValue = "deleteValue";
			const string ActionSetValue = "setValue";
			const string ActionCreateValue = "createJsonPointerValue";
			const string ActionReplaceJsonValue = "replaceJsonValue";
			const string ActionReadValue = "readValue";
			const string ActionAppendArray = "appendArray";
			const string ActionInsertArray = "insertArray";
			const string ActionRemoveArrayElement = "removeArrayElement";
			const string ActionDistinctValues = "distinctValues";

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
					case ActionAppendArray:
						flags |= (int)JsonFlags.AppendArray;
						action = (int)JsonAction.AppendArray;
						break;
					case ActionInsertArray:
						flags |= (int)JsonFlags.InsertArray;
						action = (int)JsonAction.InsertArray;
						break;
					case ActionRemoveArrayElement:
						flags |= (int)JsonFlags.RemoveArrayElement;
						action = (int)JsonAction.RemoveArrayElement;
						break;
					case ActionDistinctValues:
						flags |= (int)JsonFlags.DistinctValues;
						action = (int)JsonAction.DistinctValues;
						break;
					default:
						Messaging.Write(ErrorMessages.IllegalAttributeValue(sourceLineNumbers, node.Name.ToString(),
							"Action", actionValue, ActionDeleteValue, ActionSetValue, ActionReplaceJsonValue, 
							ActionCreateValue, ActionReadValue, ActionAppendArray, ActionInsertArray, ActionRemoveArrayElement, ActionDistinctValues));
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
			else if ((action == (int)JsonAction.SetValue || action == (int)JsonAction.ReplaceJsonValue || 
			         action == (int)JsonAction.CreateJsonPointerValue) && string.IsNullOrEmpty(value))
			{
				// These actions require Value attribute
				string actionName = action == (int)JsonAction.SetValue ? "setValue" :
				                   action == (int)JsonAction.ReplaceJsonValue ? "replaceJsonValue" : "createJsonPointerValue";
				Messaging.Write(ErrorMessages.ExpectedAttribute(sourceLineNumbers, node.Name.ToString(), "Value", "Action", actionName));
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
		internal bool ContainsUnescapedBrackets(string path)
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
						if (NumericIndexPattern.IsMatch(content) || content == "*" || 
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
			// This is not a complete JSONPath parser, just catches the most obvious mistakes

			// Check for invalid characters immediately after $
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
			var matches = PropertyReferencePattern.Matches(attributeValue);

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

		/// <summary>
		/// Validates that a string only contains valid property name characters (alphanumeric, underscore, dot).
		/// </summary>
		internal bool IsValidPropertyName(string name)
		{
			if (string.IsNullOrEmpty(name))
			{
				return false;
			}

			// Check if the name contains only alphanumeric characters, dots, and underscores
			foreach (char c in name)
			{
				if (!char.IsLetterOrDigit(c) && c != '.' && c != '_')
				{
					return false;
				}
			}

			return true;
		}

		/// <summary>
		/// Parses a JsonTransaction element that groups multiple JsonFile operations.
		/// </summary>
		private void ParseJsonTransactionElement(Intermediate intermediate, XElement node, string componentId, string parentDirectory,
			IntermediateSection section)
		{
			var sourceLineNumbers = ParseHelper.GetSourceLineNumbers(node);
			string id = null;
			string defaultFile = null;
			int? baseSequence = 1;

			// Parse attributes
			foreach (var attribute in node.Attributes())
			{
				if (string.IsNullOrEmpty(attribute.Name.NamespaceName) || Namespace == attribute.Name.Namespace)
				{
					switch (attribute.Name.LocalName)
					{
						case "Id":
							id = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
							break;
						case "File":
							defaultFile = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
							break;
						case "BaseSequence":
							baseSequence = ToNullableInt(ParseHelper.GetAttributeValue(sourceLineNumbers, attribute));
							break;
						default:
							ParseHelper.UnexpectedAttribute(node, attribute);
							break;
					}
				}
			}

			// Validate required attributes
			if (string.IsNullOrEmpty(id))
			{
				Messaging.Write(ErrorMessages.ExpectedAttribute(sourceLineNumbers, node.Name.ToString(), "Id"));
			}

			if (Messaging.EncounteredError)
			{
				return;
			}

			// Parse child JsonFile elements. Defaults from the transaction (File, sequence based
			// on BaseSequence) are passed as overrides instead of mutating the source XML; the
			// child's own attributes always win.
			int sequenceOffset = 0;
			int childCount = 0;
			foreach (var child in node.Elements())
			{
				if (child.Name.Namespace == Namespace && child.Name.LocalName == "JsonFile")
				{
					childCount++;

					int? childSequence = baseSequence.HasValue ? baseSequence.Value + sequenceOffset : (int?)null;
					sequenceOffset++;

					ParseJsonFileElement(intermediate, child, componentId, parentDirectory, section, defaultFile, childSequence);
				}
				else if (child.Name.Namespace == Namespace)
				{
					ParseHelper.UnexpectedElement(node, child);
				}
			}

			// Validate that at least one child element exists
			if (childCount == 0)
			{
				Messaging.Write(ErrorMessages.ExpectedElement(sourceLineNumbers, node.Name.ToString(), "JsonFile"));
			}
		}

		/// <summary>
		/// Parses an AppSettings composite element for .NET application settings.
		/// </summary>
		private void ParseAppSettingsElement(XElement node, string componentId, string parentDirectory,
			IntermediateSection section)
		{
			var sourceLineNumbers = ParseHelper.GetSourceLineNumbers(node);
			Identifier id = null;
			string file = null;
			string key = null;
			string value = null;
			int? sequence = null;
			bool createIfMissing = true;

			// Parse attributes
			foreach (var attribute in node.Attributes())
			{
				if (string.IsNullOrEmpty(attribute.Name.NamespaceName) || Namespace == attribute.Name.Namespace)
				{
					switch (attribute.Name.LocalName)
					{
						case "Id":
							id = ParseHelper.GetAttributeIdentifier(sourceLineNumbers, attribute);
							break;
						case "File":
							file = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
							break;
						case "Key":
							key = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
							break;
						case "Value":
							value = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
							break;
						case "Sequence":
							sequence = ToNullableInt(ParseHelper.GetAttributeValue(sourceLineNumbers, attribute));
							break;
						case "CreateIfMissing":
							string createValue = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
							if (string.IsNullOrEmpty(createValue))
							{
								createIfMissing = true; // Default to true if not specified
							}
							else if (createValue.Equals("yes", System.StringComparison.OrdinalIgnoreCase))
							{
								createIfMissing = true;
							}
							else if (createValue.Equals("no", System.StringComparison.OrdinalIgnoreCase))
							{
								createIfMissing = false;
							}
							else
							{
								Messaging.Write(ErrorMessages.IllegalAttributeValue(sourceLineNumbers, node.Name.ToString(),
									"CreateIfMissing", createValue, "yes", "no"));
								createIfMissing = false; // Default to false for invalid value
							}
							break;
						default:
							ParseHelper.UnexpectedAttribute(node, attribute);
							break;
					}
				}
			}

			// Validate required attributes
			if (null == id)
			{
				Messaging.Write(ErrorMessages.ExpectedAttribute(sourceLineNumbers, node.Name.ToString(), "Id"));
			}
			if (string.IsNullOrEmpty(file))
			{
				Messaging.Write(ErrorMessages.ExpectedAttribute(sourceLineNumbers, node.Name.ToString(), "File"));
			}
			if (string.IsNullOrEmpty(key))
			{
				Messaging.Write(ErrorMessages.ExpectedAttribute(sourceLineNumbers, node.Name.ToString(), "Key"));
			}
			if (string.IsNullOrEmpty(value))
			{
				Messaging.Write(ErrorMessages.ExpectedAttribute(sourceLineNumbers, node.Name.ToString(), "Value"));
			}

			if (Messaging.EncounteredError)
			{
				return;
			}

			// Validate key format - warn if it contains special JSONPath characters
			if (!string.IsNullOrEmpty(key) && !IsValidPropertyName(key))
			{
				Messaging.Write(WarningMessages.InvalidPropertyNameCharacters(sourceLineNumbers, node.Name.ToString(), "Key", key));
			}

			// CreateIfMissing=yes (the default) must create the key when it is absent, which the
			// setValue action cannot do (it fails on missing paths). Map it to the
			// createJsonPointerValue action, which sets the value and creates intermediate
			// objects as needed. The dot-notation key converts to a JSON Pointer,
			// e.g. "ApplicationSettings.Environment" -> "/ApplicationSettings/Environment".
			// CreateIfMissing=no keeps setValue (JSONPath) with OnlyIfExists so a missing key
			// is skipped rather than failing the install.
			string elementPath;
			int flags;
			if (createIfMissing)
			{
				elementPath = "/" + key.Replace(".", "/");
				flags = (int)JsonFlags.CreateJsonPointerValue;
			}
			else
			{
				elementPath = "$." + key;
				flags = (int)JsonFlags.SetValue | (int)JsonFlags.OnlyIfExists;
			}

			sequence = ResolveSequence(sequence);

			var symbol = section.AddSymbol(new JsonFileSymbol(sourceLineNumbers, id)
			{
				File = file,
				ElementPath = elementPath,
				Value = value,
				Flags = flags,
				ComponentRef = componentId,
				Sequence = sequence,
				On = (int)JsonTiming.Install
			});

			ParseHelper.CreateCustomActionReference(sourceLineNumbers, section, "WixSchedJsonFile", Context.Platform,
				CustomActionPlatforms.X86 | CustomActionPlatforms.X64 | CustomActionPlatforms.ARM64);
		}

		/// <summary>
		/// Parses a ConnectionString composite element for .NET connection strings.
		/// </summary>
		private void ParseConnectionStringElement(XElement node, string componentId, string parentDirectory,
			IntermediateSection section)
		{
			var sourceLineNumbers = ParseHelper.GetSourceLineNumbers(node);
			Identifier id = null;
			string file = null;
			string name = null;
			string value = null;
			int? sequence = null;

			// Parse attributes
			foreach (var attribute in node.Attributes())
			{
				if (string.IsNullOrEmpty(attribute.Name.NamespaceName) || Namespace == attribute.Name.Namespace)
				{
					switch (attribute.Name.LocalName)
					{
						case "Id":
							id = ParseHelper.GetAttributeIdentifier(sourceLineNumbers, attribute);
							break;
						case "File":
							file = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
							break;
						case "Name":
							name = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
							break;
						case "Value":
							value = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
							break;
						case "Sequence":
							sequence = ToNullableInt(ParseHelper.GetAttributeValue(sourceLineNumbers, attribute));
							break;
						default:
							ParseHelper.UnexpectedAttribute(node, attribute);
							break;
					}
				}
			}

			// Validate required attributes
			if (null == id)
			{
				Messaging.Write(ErrorMessages.ExpectedAttribute(sourceLineNumbers, node.Name.ToString(), "Id"));
			}
			if (string.IsNullOrEmpty(file))
			{
				Messaging.Write(ErrorMessages.ExpectedAttribute(sourceLineNumbers, node.Name.ToString(), "File"));
			}
			if (string.IsNullOrEmpty(name))
			{
				Messaging.Write(ErrorMessages.ExpectedAttribute(sourceLineNumbers, node.Name.ToString(), "Name"));
			}
			if (string.IsNullOrEmpty(value))
			{
				Messaging.Write(ErrorMessages.ExpectedAttribute(sourceLineNumbers, node.Name.ToString(), "Value"));
			}

			if (Messaging.EncounteredError)
			{
				return;
			}

			// Create JSONPath for ConnectionStrings section
			// Validate name format - warn if it contains special JSONPath characters
			if (!string.IsNullOrEmpty(name) && !IsValidPropertyName(name))
			{
				Messaging.Write(WarningMessages.InvalidPropertyNameCharacters(sourceLineNumbers, node.Name.ToString(), "Name", name));
			}

			string elementPath = $"$.ConnectionStrings.{name}";

			// Create underlying JsonFile symbol with setValue action
			int flags = (int)JsonFlags.SetValue;

			sequence = ResolveSequence(sequence);

			var symbol = section.AddSymbol(new JsonFileSymbol(sourceLineNumbers, id)
			{
				File = file,
				ElementPath = elementPath,
				Value = value,
				Flags = flags,
				ComponentRef = componentId,
				Sequence = sequence,
				On = (int)JsonTiming.Install
			});

			ParseHelper.CreateCustomActionReference(sourceLineNumbers, section, "WixSchedJsonFile", Context.Platform,
				CustomActionPlatforms.X86 | CustomActionPlatforms.X64 | CustomActionPlatforms.ARM64);
		}

		/// <summary>
		/// Parses a LoggingLevel composite element for .NET logging configuration.
		/// </summary>
		private void ParseLoggingLevelElement(XElement node, string componentId, string parentDirectory,
			IntermediateSection section)
		{
			var sourceLineNumbers = ParseHelper.GetSourceLineNumbers(node);
			Identifier id = null;
			string file = null;
			string category = "Default";
			string level = null;
			int? sequence = null;

			// Parse attributes
			foreach (var attribute in node.Attributes())
			{
				if (string.IsNullOrEmpty(attribute.Name.NamespaceName) || Namespace == attribute.Name.Namespace)
				{
					switch (attribute.Name.LocalName)
					{
						case "Id":
							id = ParseHelper.GetAttributeIdentifier(sourceLineNumbers, attribute);
							break;
						case "File":
							file = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
							break;
						case "Category":
							category = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
							if (string.IsNullOrEmpty(category))
							{
								category = "Default";
							}
							break;
						case "Level":
							level = ParseHelper.GetAttributeValue(sourceLineNumbers, attribute);
							break;
						case "Sequence":
							sequence = ToNullableInt(ParseHelper.GetAttributeValue(sourceLineNumbers, attribute));
							break;
						default:
							ParseHelper.UnexpectedAttribute(node, attribute);
							break;
					}
				}
			}

			// Validate required attributes
			if (null == id)
			{
				Messaging.Write(ErrorMessages.ExpectedAttribute(sourceLineNumbers, node.Name.ToString(), "Id"));
			}
			if (string.IsNullOrEmpty(file))
			{
				Messaging.Write(ErrorMessages.ExpectedAttribute(sourceLineNumbers, node.Name.ToString(), "File"));
			}
			if (string.IsNullOrEmpty(level))
			{
				Messaging.Write(ErrorMessages.ExpectedAttribute(sourceLineNumbers, node.Name.ToString(), "Level"));
			}

			if (Messaging.EncounteredError)
			{
				return;
			}

			// Create JSONPath for Logging.LogLevel section
			// Validate category format - warn if it contains special JSONPath characters
			if (!string.IsNullOrEmpty(category) && !IsValidPropertyName(category))
			{
				Messaging.Write(WarningMessages.InvalidPropertyNameCharacters(sourceLineNumbers, node.Name.ToString(), "Category", category));
			}

			string elementPath = $"$.Logging.LogLevel.{category}";

			// Create underlying JsonFile symbol with setValue action
			int flags = (int)JsonFlags.SetValue;

			sequence = ResolveSequence(sequence);

			var symbol = section.AddSymbol(new JsonFileSymbol(sourceLineNumbers, id)
			{
				File = file,
				ElementPath = elementPath,
				Value = level,
				Flags = flags,
				ComponentRef = componentId,
				Sequence = sequence,
				On = (int)JsonTiming.Install
			});

			ParseHelper.CreateCustomActionReference(sourceLineNumbers, section, "WixSchedJsonFile", Context.Platform,
				CustomActionPlatforms.X86 | CustomActionPlatforms.X64 | CustomActionPlatforms.ARM64);
		}
	}

}
