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

The following secrets must be configured in the repository:
- `NUGET_API_KEY`: API key for publishing to NuGet.org

## Notes

- The `--skip-duplicate` flag prevents errors if the package version already exists on NuGet
- GitHub releases are created as non-draft, non-prerelease by default
- The NuGet package is also attached as an asset to the GitHub release
