# Windows Alpha release support

The setup executable, manifest, core archive, and every UCAS part belong to one
GitHub release tag. `ReleaseConfiguration.ReleaseTag` is the installer source of
truth; the downloaded manifest must contain the same tag or installation stops.

Public binary distribution is currently withheld. Do not publish or link a
playable build until the OH-58 source rights and the unresolved supplied-audio
provenance are cleared and the final package passes the complete release test.

## Required release assets

Upload these assets to the single configured release tag:

- `ProjectRotorlineAlphaSetup.exe`
- `Rotorline-Alpha-Windows.manifest.json`
- `Rotorline-Alpha-Windows-Core.zip`
- every manifest-listed `Rotorline-Windows.ucas.partN` file

The core archive must contain these exact paths:

- `Rotorline.exe`
- `Rotorline/Content/Paks/Rotorline-Windows.pak`
- `Rotorline/Content/Paks/Rotorline-Windows.utoc`
- `Prerequisites/VC_redist.x64.exe`
- `Prerequisites/GameInputRedist.msi`

`Rotorline-Windows.ucas` must not be inside the core archive. Split it into any
positive number of contiguous parts beginning with `part1`. The manifest
generator enumerates them dynamically and records every part's size and SHA-256,
plus the assembled UCAS size and SHA-256.

## Build and local self-test

```powershell
& .\Build-Setup.ps1
```

The setup is written outside this repository to
`E:\games\ProjectRotorlineInstallerBuild`. The default build runs a synthetic
test that uses no Rotorline package binaries. It verifies a three-part UCAS,
tamper rejection, sibling staging, rollback, shortcut readback, and exact
executable launch survival.

## Generate the final manifest

After producing the final package and core archive in a release workspace:

```powershell
& .\New-ReleaseManifest.ps1 `
  -ReleaseTag 'alpha-windows-v1' `
  -CoreArchivePath 'X:\release\Rotorline-Alpha-Windows-Core.zip' `
  -UcasPartsDirectory 'X:\release' `
  -OutputPath 'X:\release\Rotorline-Alpha-Windows.manifest.json'
```

The generator rejects missing/gapped parts, an embedded UCAS, or missing exact
game/prerequisite paths. It calculates hashes directly rather than accepting
operator-entered values.

## Final release gate

Before uploading anything:

1. Clear every publication hold documented in the project credits.
2. Build the final Shipping package and include the exact x64 VC and GameInput
   prerequisite installers at the required paths.
3. Generate the manifest and build the setup for the same single release tag.
4. Install on a clean Windows test account with an older install already present.
5. Confirm prerequisite exit codes, atomic replacement, rollback on forced
   failure, shortcut readback, and the exact installed `Rotorline.exe` surviving
   launch verification.
6. Compare all release asset names and hashes to the generated manifest.
7. Only then upload the complete asset set and add a playable README link.
