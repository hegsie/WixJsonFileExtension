# Code signing policy

Free code signing provided by [SignPath.io](https://about.signpath.io/), certificate by [SignPath Foundation](https://signpath.org/).

## What is signed

The native custom action DLL `jsonca.dll` (x64, x86 and ARM64) that the extension embeds in installers. It is built from the source in this repository by the [release workflow](.github/workflows/release.yml) on GitHub Actions, and signed only for tagged releases. Each signed file carries the product name `WixJsonFileExtension` and the release version in its version resource.

Only binaries built from this repository's source are signed. Installers that consume the extension are built and signed by their own authors, not by this project.

## Team roles

- Committers and reviewers: [hegsie](https://github.com/hegsie) (repository owner; the only account with write access)
- Approvers: [hegsie](https://github.com/hegsie)

Outside contributors submit changes as pull requests from forks; they cannot push to this repository or approve signing.

Every change reaches `main` through a reviewed pull request, and every signing request is approved manually in SignPath. All team members use multi-factor authentication on GitHub and SignPath.

## Privacy policy

This program will not transfer any information to other networked systems unless specifically requested by the user or the person installing or operating it.

The custom action only reads and writes the JSON files named in the installer's authoring, on the machine where the installer runs.
