# Release Process

This document describes the release process for WixJsonFileExtension.

## Overview

The project now uses a separate release workflow to publish packages to NuGet and create GitHub releases. The CI/CD build workflow (MSBuild) no longer automatically pushes to NuGet.

## Workflows

### MSBuild Workflow (`.github/workflows/msbuild.yml`)
- **Triggers**: On push to `main` branch or pull requests to `main`
- **Purpose**: Continuous integration - builds, tests, and packages the code
- **Artifacts**: Creates NuGet package as a build artifact (not published to NuGet)

### Release Workflow (`.github/workflows/release.yml`)
- **Triggers**: 
  - Manual workflow dispatch with version input
  - Push of version tags (format: `v*.*.*`, e.g., `v6.0.1`)
- **Purpose**: Create formal releases and publish to NuGet
- **Actions**:
  1. Builds the project
  2. Creates NuGet package with specified version
  3. Creates a GitHub release
  4. Uploads NuGet package as release asset
  5. Publishes package to NuGet.org

## Creating a Release

### Option 1: Manual Release (Recommended)
1. Go to the GitHub repository's "Actions" tab
2. Select the "Release" workflow
3. Click "Run workflow"
4. Enter the version number (e.g., `6.0.1`)
5. Click "Run workflow"

The workflow will:
- Build and package the code
- Create a git tag `v6.0.1` if it doesn't exist
- Create a GitHub release with the package
- Publish to NuGet.org

### Option 2: Tag-based Release
1. Create and push a version tag:
   ```bash
   git tag v6.0.1
   git push origin v6.0.1
   ```
2. The release workflow will automatically trigger

## Version Numbering

- Use semantic versioning: `MAJOR.MINOR.PATCH`
- Example: `6.0.1`, `6.1.0`, `7.0.0`
- Do not include the `v` prefix when entering version in workflow dispatch (it will be added automatically)

## Prerequisites

Publishing to NuGet.org uses [Trusted Publishing](https://learn.microsoft.com/en-us/nuget/nuget-org/trusted-publishing) instead of a stored API key. The workflow exchanges its GitHub OIDC token for a short-lived NuGet.org API key via the `NuGet/login` action, so no long-lived secret exists to leak or expire.

A Trusted Publishing policy must be configured on nuget.org under the `hegsie` account (nuget.org → account → Trusted Publishing):
- **Repository owner**: `hegsie`
- **Repository**: `WixJsonFileExtension`
- **Workflow file**: `release.yml`

NuGet publishing requires no repository secrets. DLL signing does require the two repository secrets below.

The release workflow Authenticode-signs the x86, x64, and ARM64 custom-action DLLs before building the wixlib that embeds them. Obtain a trusted code-signing certificate that includes an exportable private key, and export it as a password-protected PKCS#12/PFX file. Keep the PFX and password private.

Encode the PFX on a trusted machine. For example, in PowerShell on Windows, replace the path with the PFX location:

```powershell
[Convert]::ToBase64String([IO.File]::ReadAllBytes("C:\path\to\certificate.pfx")) | Set-Clipboard
```

In this repository on GitHub, open **Settings → Secrets and variables → Actions → Repository secrets**, then select **New repository secret** for each:
- `WINDOWS_SIGNING_CERTIFICATE_BASE64`: Base64-encoded PFX file
- `WINDOWS_SIGNING_CERTIFICATE_PASSWORD`: PFX password

Paste the encoded value from the clipboard as the first secret and the PFX password as the second. The repository's Settings tab and secrets management require suitable repository permissions. Do not print either value in workflow logs or commit them to the repository.

The workflow signs with SHA-256, adds a trusted timestamp, and verifies each DLL signature. A release fails if the secrets are missing or signing/verification fails. Keep the certificate and password private; do not commit them to the repository. This signs the extension's custom-action DLLs, not an installer's final MSI or bundle; projects consuming the extension should sign their own final installer with their own certificate.

## Notes

- The `--skip-duplicate` flag prevents errors if the package version already exists on NuGet
- GitHub releases are created as non-draft, non-prerelease by default
- The NuGet package is also attached as an asset to the GitHub release
