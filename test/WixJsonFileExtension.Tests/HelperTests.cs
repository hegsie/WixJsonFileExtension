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

        [Theory]
        [InlineData("$.store.book")]                 // no brackets
        [InlineData(@"$.Books[\[]0[\]].Title")]      // MSI-escaped brackets
        public void ContainsUnescapedBrackets_AllowsEscapedOrNoBrackets(string path)
        {
            Assert.False(_compiler.ContainsUnescapedBrackets(path));
        }
    }
}
