using System;
using System.IO;
using Hegsie.Wix.JsonExtension;
using WixToolset.Core.TestPackage;
using Xunit;

namespace WixJsonFileExtension.Tests
{
    // Frontend (compile-to-.wixipl) tests that exercise the JsonCompiler through the real WiX
    // pipeline using WixToolset.Core.TestPackage. They assert on build success/failure, which is the
    // least brittle way to verify validation behaviour. Compiling to .wixipl runs only the frontend,
    // so these do not need the native jsonca.dll binary or a full MSI link.
    //
    // NOTE: the exact WixRunner/TestPackage API and package version may need adjusting on the first
    // Windows build/restore; the assertions below are intentionally simple (exit-code based).
    public class CompilerTests
    {
        private static string ExtensionPath => typeof(JsonCompiler).Assembly.Location;

        private static string TestDataDir(string name) =>
            Path.Combine(AppContext.BaseDirectory, "TestData", name);

        private static WixRunnerResult Compile(string testDataFolder)
        {
            var sourceDir = TestDataDir(testDataFolder);
            var sourcePath = Path.Combine(sourceDir, "Package.wxs");
            var outputFolder = Path.Combine(Path.GetTempPath(), "WixJsonTests", Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(outputFolder);

            return WixRunner.Execute(new[]
            {
                "build",
                sourcePath,
                "-ext", ExtensionPath,
                "-intermediateFolder", Path.Combine(outputFolder, "obj"),
                "-arch", "x64",
                "-o", Path.Combine(outputFolder, "test.wixipl")
            });
        }

        [Fact]
        public void ValidJsonFileAuthoringCompiles()
        {
            var result = Compile("ValidJsonFile");
            Assert.Equal(0, result.ExitCode);
        }

        [Fact]
        public void SetValueWithoutValueFailsToCompile()
        {
            // setValue requires a Value attribute; the compiler must surface an error (regression
            // guard for the default-action / discarded-error fixes).
            var result = Compile("MissingValue");
            Assert.NotEqual(0, result.ExitCode);
        }

        [Fact]
        public void UnsupportedChildElementFailsToCompile()
        {
            // A non-extension child element under JsonFile must be reported, not silently ignored.
            var result = Compile("UnsupportedChild");
            Assert.NotEqual(0, result.ExitCode);
        }
    }
}
