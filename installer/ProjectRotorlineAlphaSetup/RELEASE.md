# Windows Alpha release support

The setup executable, manifest, core archive, and every UCAS part belong to one
GitHub release tag. `ReleaseConfiguration.ReleaseTag` is the installer source of
truth; the downloaded manifest must contain the same tag or installation stops.

OH-58/Kiowa distribution permission is confirmed. Keith League created and owns
the original Project Rotorline audio identified during the audit. No rights hold
remains for those assets. Do not publish or link a playable build until the final
package passes the complete release test.

## Current qualified release

- Tag: `alpha-windows-v1`
- Published: August 8, 2026
- Setup SHA-256: `821D7AA7163F40EF886C0ECB5D6AB6006106A8740E897EADB6B3EA240D1EE142`
- Five same-tag assets: setup, manifest, core ZIP, and two UCAS parts
- GitHub round-trip hashes and anonymous setup/manifest URLs: passed
- Core archive membership: 44/44 files, no embedded UCAS
- Reassembled UCAS: 2,205,772,016 bytes, canonical SHA-256 verified
- Isolated real-payload install, forced rollback, second activation, shortcut
  readback, exact Shipping startup, and runtime probe: passed
- Installed acceptance tree: 45 files, 3,761,445,813 bytes
- Smooth Operator qualification correction included: practical 2+ m AGL hover,
  up to 9 km/h modeled drift, 0.75-second jitter tolerance, and visible sortie
  telemetry for safe landing and hover duration.

The setup is not code-signed, so Windows SmartScreen may warn. Both packaged
Microsoft prerequisites have valid Microsoft signatures.

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

1. Confirm the project credits still record the project owner's rights clearance.
2. Build the final Shipping package and include the exact x64 VC and GameInput
   prerequisite installers at the required paths.
3. Generate the manifest and build the setup for the same single release tag.
4. Install on a clean Windows test account with an older install already present.
5. Confirm prerequisite exit codes, atomic replacement, rollback on forced
   failure, shortcut readback, and the exact installed `Rotorline.exe` surviving
   launch verification.
6. Compare all release asset names and hashes to the generated manifest.
7. Only then upload the complete asset set and add a playable README link.
