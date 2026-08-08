$ErrorActionPreference = 'Stop'

$Root = Join-Path (
    [System.IO.Path]::GetTempPath()
) ("ProjectRotorlineManifestTest-{0}" -f [guid]::NewGuid().ToString('N'))
$CoreRoot = Join-Path $Root 'core'
$PartsRoot = Join-Path $Root 'parts'
$CoreArchive = Join-Path $Root 'Rotorline-Alpha-Windows-Core.zip'
$ManifestPath = Join-Path $Root 'Rotorline-Alpha-Windows.manifest.json'
$ResolvedTemp = [System.IO.Path]::GetFullPath(
    [System.IO.Path]::GetTempPath()
).TrimEnd('\') + '\'
$ResolvedRoot = [System.IO.Path]::GetFullPath($Root)
if (-not $ResolvedRoot.StartsWith(
    $ResolvedTemp,
    [StringComparison]::OrdinalIgnoreCase
) -or (Split-Path -Leaf $ResolvedRoot) -notlike 'ProjectRotorlineManifestTest-*') {
    throw "Unsafe manifest-test root: $ResolvedRoot"
}

function Write-TestFile {
    param(
        [string]$RelativePath,
        [string]$Content
    )

    $Path = Join-Path $CoreRoot $RelativePath
    $Directory = Split-Path -Parent $Path
    New-Item -ItemType Directory -Path $Directory -Force | Out-Null
    [System.IO.File]::WriteAllText(
        $Path,
        $Content,
        (New-Object System.Text.UTF8Encoding($false))
    )
}

try {
    New-Item -ItemType Directory -Path $CoreRoot, $PartsRoot -Force | Out-Null
    Write-TestFile -RelativePath 'Rotorline.exe' -Content 'synthetic executable'
    Write-TestFile -RelativePath 'Rotorline\Content\Paks\Rotorline-Windows.pak' -Content 'synthetic pak'
    Write-TestFile -RelativePath 'Rotorline\Content\Paks\Rotorline-Windows.utoc' -Content 'synthetic utoc'
    Write-TestFile -RelativePath 'Prerequisites\VC_redist.x64.exe' -Content 'synthetic vc redist'
    Write-TestFile -RelativePath 'Prerequisites\GameInputRedist.msi' -Content 'synthetic gameinput'

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::CreateFromDirectory(
        $CoreRoot,
        $CoreArchive,
        [System.IO.Compression.CompressionLevel]::Optimal,
        $false
    )

    1..3 | ForEach-Object {
        [System.IO.File]::WriteAllText(
            (Join-Path $PartsRoot "Rotorline-Windows.ucas.part$_"),
            "synthetic ucas part $_",
            (New-Object System.Text.UTF8Encoding($false))
        )
    }

    $null = & (Join-Path $PSScriptRoot 'New-ReleaseManifest.ps1') `
        -ReleaseTag 'alpha-windows-v1' `
        -CoreArchivePath $CoreArchive `
        -UcasPartsDirectory $PartsRoot `
        -OutputPath $ManifestPath

    $Manifest = Get-Content -Raw -LiteralPath $ManifestPath | ConvertFrom-Json
    if ($Manifest.releaseTag -cne 'alpha-windows-v1') {
        throw 'Generated manifest release tag mismatch.'
    }
    if (@($Manifest.ucas.parts).Count -ne 3) {
        throw 'Generated manifest did not enumerate all three UCAS parts.'
    }
    if (@($Manifest.installedFiles).Count -ne 6) {
        throw 'Generated manifest did not contain the six exact installed files.'
    }
    if ([string]$Manifest.ucas.assembledSha256 -notmatch '^[0-9a-f]{64}$') {
        throw 'Generated manifest assembled SHA-256 is invalid.'
    }

    Move-Item `
        -LiteralPath (Join-Path $PartsRoot 'Rotorline-Windows.ucas.part2') `
        -Destination (Join-Path $PartsRoot 'Rotorline-Windows.ucas.part4')
    $GapRejected = $false
    try {
        $null = & (Join-Path $PSScriptRoot 'New-ReleaseManifest.ps1') `
            -ReleaseTag 'alpha-windows-v1' `
            -CoreArchivePath $CoreArchive `
            -UcasPartsDirectory $PartsRoot `
            -OutputPath $ManifestPath
    } catch {
        $GapRejected = $true
    }
    if (-not $GapRejected) {
        throw 'Manifest generator accepted a gapped UCAS part sequence.'
    }

    Write-Output 'PASS: release manifest generator enumerated three parts, emitted exact file hashes, and rejected a part gap.'
} finally {
    if (Test-Path -LiteralPath $ResolvedRoot) {
        Remove-Item -LiteralPath $ResolvedRoot -Recurse -Force
    }
}
