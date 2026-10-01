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

NuGet publishing requires no repository secrets.

### Code signing (SignPath Foundation)

The release workflow Authenticode-signs the custom action DLLs (`jsonca.dll` for x64, x86 and ARM64) before the wixlib embeds them, so every MSI built with the extension carries signed custom actions (issue #56). Signing uses [SignPath Foundation](https://signpath.org/), which provides certificates and signing free of charge to open source projects. The private key stays in SignPath's HSM, so no certificate or password is stored in this repository.

The signing steps are skipped while the `SIGNPATH_ORGANIZATION_ID` repository variable is unset, so releases still build (unsigned) until signing is configured. Once it is set, a release fails if signing fails or if any DLL is not validly signed after the build.

One-time setup:

1. Apply at <https://signpath.org/apply> and wait for approval.
2. In SignPath, create:
   - a project with slug `WixJsonFileExtension`, with GitHub.com added as a trusted build system for this repository;
   - an artifact configuration with slug `custom-action-dlls` using [`.signpath/artifact-configuration.xml`](../.signpath/artifact-configuration.xml);
   - a signing policy with slug `release-signing`;
   - a CI user, and an API token for it with submitter permission on that policy.
3. In this repository, under **Settings → Secrets and variables → Actions**, add:
   - variable `SIGNPATH_ORGANIZATION_ID`: the organization ID shown in SignPath;
   - secret `SIGNPATH_API_TOKEN`: the CI user's API token.

This signs the extension's custom-action DLLs, not an installer's final MSI or bundle; projects consuming the extension should sign their own final installer with their own certificate.

## Notes

- The `--skip-duplicate` flag prevents errors if the package version already exists on NuGet
- GitHub releases are created as non-draft, non-prerelease by default
- The NuGet package is also attached as an asset to the GitHub release
