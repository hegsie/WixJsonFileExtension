# Contributing to WixJsonFileExtension

Thank you for your interest in contributing to WixJsonFileExtension! We welcome contributions from the community.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [How to Contribute](#how-to-contribute)
- [Reporting Issues](#reporting-issues)
- [Development Workflow](#development-workflow)
- [Testing](#testing)
- [Pull Request Process](#pull-request-process)

## Code of Conduct

Please be respectful and constructive in all interactions. We aim to maintain a welcoming and inclusive community.

## Getting Started

1. Fork the repository on GitHub
2. Clone your fork locally:
   ```bash
   git clone https://github.com/YOUR-USERNAME/WixJsonFileExtension.git
   cd WixJsonFileExtension
   ```
3. Add the upstream repository as a remote:
   ```bash
   git remote add upstream https://github.com/hegsie/WixJsonFileExtension.git
   ```

## How to Contribute

### Types of Contributions

- **Bug fixes**: Fix issues reported in the issue tracker
- **New features**: Add new functionality (please discuss in an issue first)
- **Documentation**: Improve or expand documentation
- **Examples**: Add new usage examples
- **Tests**: Add test cases for existing functionality

## Reporting Issues

When reporting bugs or requesting features, please use the GitHub issue tracker and include:

### For Bug Reports

- **WiX Toolset version** (e.g., WiX 5.0, WiX 6.0)
- **Extension version** (from NuGet package version)
- **.NET version** (if relevant)
- **Minimal reproduction example**: A small WiX project that demonstrates the issue
- **JSON structure**: The JSON file you're working with
- **Expected behavior**: What you expected to happen
- **Actual behavior**: What actually happened
- **MSI log excerpts**: Relevant error messages from the MSI log file

### For Feature Requests

- **Use case**: Describe the problem you're trying to solve
- **Proposed solution**: How you envision the feature working
- **Alternatives considered**: Other approaches you've thought about
- **Additional context**: Any other relevant information

## Development Workflow

### Prerequisites

- **Visual Studio 2022** or later (with C++ and .NET workloads)
- **WiX Toolset v4 or later** installed
- **.NET SDK 6.0 or later**
- **Windows 10/11** or Windows Server

### Building the Project

1. Restore NuGet packages:
   ```bash
   nuget restore WixJsonFileExtension.sln
   ```
   or
   ```bash
   dotnet restore WixJsonFileExtension.sln
   ```

2. Build the solution:
   ```bash
   msbuild /m /p:Configuration=Release WixJsonFileExtension.sln
   ```

3. The output will be in:
   - Extension DLL: `src/wixext/bin/Release/netstandard2.0/WixJsonFileExtension.dll`
   - NuGet package: `src/wixext/bin/Release/WixJsonFileExtension.{version}.nupkg`
   - Custom action DLL: `src/ca/bin/Release/jsonca.dll`

### Project Structure

```
WixJsonFileExtension/
├── .github/
│   └── workflows/          # CI/CD workflows
│       ├── msbuild.yml     # Build and package workflow
│       └── regression-tests.yml  # Automated testing workflow
├── src/
│   ├── ca/                 # Native C++ custom action
│   │   └── jsoncons/       # JSON library (jsoncons)
│   ├── wixext/             # C# WiX extension
│   │   ├── JsonCompiler.cs # Compiles JsonFile elements
│   │   └── Table/          # Symbol definitions
│   └── wixlib/             # WiX library with custom action definitions
├── TestJsonConfigInstaller/ # Example installer project
│   ├── Product.wxs         # Original example
│   ├── DotNetPatterns.wxs  # .NET configuration patterns example
│   └── *.json              # Test JSON files
└── README.md
```

### Coding Standards

- **C# code**: Follow standard C# conventions
- **C++ code**: Follow existing code style in the project
- **WiX XML**: Use proper formatting and indentation
- **Comments**: Add comments for complex logic or non-obvious behavior
- **Documentation**: Update README.md for user-facing changes

## Testing

### Manual Testing

1. Build the solution
2. Test with the example installers in `TestJsonConfigInstaller/`
3. Create a test MSI:
   ```bash
   cd TestJsonConfigInstaller
   msbuild /p:Configuration=Release TestJsonConfigInstaller.wixproj
   ```
4. Install and verify the JSON modifications:
   ```bash
   msiexec /i bin\Release\en-US\TestJsonConfigInstaller.msi /l*v install.log
   ```

### Automated Testing

The project includes regression tests that run automatically on GitHub Actions:

- **Build validation**: Ensures the solution builds successfully
- **JSON validation**: Verifies JSON files are well-formed
- **.NET patterns**: Tests common configuration patterns
- **JSONPath patterns**: Validates path query scenarios

To run tests locally, you can use the regression test script patterns from `.github/workflows/regression-tests.yml`.

### Test Coverage Areas

When adding new features, consider adding tests for:

- **Valid inputs**: Normal usage scenarios
- **Edge cases**: Unusual but valid inputs
- **Error handling**: Invalid inputs should fail gracefully
- **Common patterns**: Typical .NET application scenarios

## Pull Request Process

1. **Create a feature branch** from `main`:
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. **Make your changes** in small, logical commits:
   - Keep commits focused and atomic
   - Write clear commit messages
   - Reference issues when applicable (e.g., "Fixes #123")

3. **Test your changes**:
   - Build the solution successfully
   - Test with the example installers
   - Verify no existing functionality is broken

4. **Update documentation**:
   - Update README.md if adding user-facing features
   - Add code comments for complex logic
   - Include usage examples for new features

5. **Push to your fork**:
   ```bash
   git push origin feature/your-feature-name
   ```

6. **Open a Pull Request**:
   - Provide a clear description of the changes
   - Reference related issues
   - Explain the motivation and use case
   - Include before/after examples if applicable

7. **Code review**:
   - Respond to feedback constructively
   - Make requested changes
   - Keep the PR updated with the main branch

8. **CI checks**:
   - Ensure all automated checks pass
   - Fix any build or test failures

### Pull Request Checklist

Before submitting your PR, ensure:

- [ ] Code builds successfully
- [ ] No new warnings introduced
- [ ] Tested manually with example installers
- [ ] Documentation updated (README.md, code comments)
- [ ] Commit messages are clear and descriptive
- [ ] PR description explains the change and motivation
- [ ] All CI checks pass

## Questions?

If you have questions about contributing, feel free to:

- Open a discussion in the GitHub Discussions tab
- Comment on a related issue
- Reach out to the maintainers

Thank you for contributing to WixJsonFileExtension!
