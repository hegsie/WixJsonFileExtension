using System.Linq;
using Hegsie.Wix.JsonExtension;
using Xunit;

namespace WixJsonFileExtension.Tests
{
    // Unit tests for the pure (state-free) helper methods on JsonCompiler. These do not require the
    // WiX compiler harness, so they run fast under `dotnet test`. The methods under test are marked
    // internal and exposed to this assembly via InternalsVisibleTo in the wixext project.
    public class HelperTests
    {
        private readonly JsonCompiler _compiler = new JsonCompiler();

        [Theory]
        [InlineData("5", 5)]
        [InlineData("-1", -1)]
        [InlineData("0", 0)]
        [InlineData("42", 42)]
        public void ToNullableInt_ParsesIntegers(string input, int expected)
        {
            Assert.Equal(expected, _compiler.ToNullableInt(input));
        }

        [Theory]
        [InlineData("")]
        [InlineData(null)]
        [InlineData("abc")]
        [InlineData("1.5")]
        [InlineData("[PROPERTY]")]
        public void ToNullableInt_ReturnsNullForNonIntegers(string input)
        {
            Assert.Null(_compiler.ToNullableInt(input));
        }

        [Theory]
        [InlineData("ValidName")]
        [InlineData("With.Dots")]
        [InlineData("With_Underscore")]
        [InlineData("Number123")]
        public void IsValidPropertyName_AcceptsValidNames(string name)
        {
            Assert.True(_compiler.IsValidPropertyName(name));
        }

        [Theory]
        [InlineData("has space")]
        [InlineData("has-dash")]
        [InlineData("has$dollar")]
        [InlineData("")]
        public void IsValidPropertyName_RejectsInvalidNames(string name)
        {
            Assert.False(_compiler.IsValidPropertyName(name));
        }

        [Theory]
        [InlineData("$.Books[0].Title")] // unescaped numeric index
        [InlineData("$.Books[*]")]       // unescaped wildcard
        public void ContainsUnescapedBrackets_DetectsUnescaped(string path)
        {
            Assert.True(_compiler.ContainsUnescapedBrackets(path));
        }

        // JsonTiming is internal, so the expected value is passed as its column value (install=1,
        // uninstall=2, both=3) rather than as the enum, which a public test signature cannot expose.
        [Theory]
        [InlineData(null, 1)]        // attribute absent
        [InlineData("", 1)]          // empty value falls back to the default
        [InlineData("install", 1)]
        [InlineData("uninstall", 2)]
        [InlineData("both", 3)]
        public void TryParseOn_AcceptsKnownValues(string value, int expected)
        {
            Assert.True(JsonCompiler.TryParseOn(value, out var timing));
            Assert.Equal((JsonTiming)expected, timing);
        }

        [Theory]
        [InlineData("Install")]   // case-sensitive, like every other WiX enumeration
        [InlineData("remove")]
        [InlineData("yes")]
        public void TryParseOn_RejectsUnknownValues(string value)
        {
            Assert.False(JsonCompiler.TryParseOn(value, out _));
        }

        [Fact]
        public void JsonFlags_BackupModifiersDoNotOverlapActionsOrOtherModifiers()
        {
            // The custom action tests bit positions 11 and 12 for these; they must stay clear of
            // every action bit (1..512) and the ValidateSchema/OnlyIfExists modifiers (256, 1024).
            Assert.Equal(2048, (int)JsonFlags.CreateBackup);
            Assert.Equal(4096, (int)JsonFlags.RestoreOnUninstall);
            var all = System.Enum.GetValues(typeof(JsonFlags)).Cast<int>().ToArray();
            Assert.Equal(all.Length, all.Distinct().Count());
            Assert.All(all, v => Assert.Equal(1, System.Numerics.BitOperations.PopCount((uint)v)));
        }

        [Fact]
        public void JsonTiming_BothIsUnionOfInstallAndUninstall()
        {
            // The custom actions test the bit for the phase they run in, so Both must carry both bits.
            Assert.True(JsonTiming.Both.HasFlag(JsonTiming.Install));
            Assert.True(JsonTiming.Both.HasFlag(JsonTiming.Uninstall));
            Assert.False(JsonTiming.Install.HasFlag(JsonTiming.Uninstall));
        }

        [Theory]
        [InlineData("$.store.book")]                 // no brackets
        [InlineData(@"$.Books[\[]0[\]].Title")]      // MSI-escaped brackets
        public void ContainsUnescapedBrackets_AllowsEscapedOrNoBrackets(string path)
        {
            Assert.False(_compiler.ContainsUnescapedBrackets(path));
        }
    }
}
