[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9._-]+$')]
    [string]$ReleaseTag,

    [Parameter(Mandatory = $true)]
    [string]$CoreArchivePath,

    [Parameter(Mandatory = $true)]
    [string]$UcasPartsDirectory,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$ExpectedCoreName = 'Rotorline-Alpha-Windows-Core.zip'
$ExpectedFiles = [ordered]@{
    gameExe = 'Rotorline.exe'
    pak = 'Rotorline/Content/Paks/Rotorline-Windows.pak'
    utoc = 'Rotorline/Content/Paks/Rotorline-Windows.utoc'
    vcRedistX64 = 'Prerequisites/VC_redist.x64.exe'
    gameInput = 'Prerequisites/GameInputRedist.msi'
}
$UcasRelativePath = 'Rotorline/Content/Paks/Rotorline-Windows.ucas'

function ConvertTo-Hex {
    param([byte[]]$Bytes)
    return -join ($Bytes | ForEach-Object { $_.ToString('x2') })
}

function Get-FileSpec {
    param([System.IO.FileInfo]$File)
    return [ordered]@{
        name = $File.Name
        size = [long]$File.Length
        sha256 = (Get-FileHash -LiteralPath $File.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

function Get-ZipEntrySpec {
    param(
        [System.IO.Compression.ZipArchive]$Archive,
        [string]$Key,
        [string]$RelativePath
    )

    $Matches = @(
        $Archive.Entries | Where-Object {
            $_.FullName.Replace('\', '/') -eq $RelativePath
        }
    )
    if ($Matches.Count -ne 1) {
        throw "Core archive must contain exactly one '$RelativePath' entry; found $($Matches.Count)."
    }

    $Entry = $Matches[0]
    $Stream = $Entry.Open()
    $Sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $Hash = ConvertTo-Hex -Bytes $Sha.ComputeHash($Stream)
    } finally {
        $Sha.Dispose()
        $Stream.Dispose()
    }

    return [ordered]@{
        key = $Key
        relativePath = $RelativePath
        size = [long]$Entry.Length
        sha256 = $Hash
    }
}

function Get-ConcatenatedHash {
    param([System.IO.FileInfo[]]$Parts)

    $Sha = [System.Security.Cryptography.SHA256]::Create()
    $Buffer = New-Object byte[] (1024 * 1024)
    [long]$Total = 0
    try {
        foreach ($Part in $Parts) {
            $Stream = $Part.OpenRead()
            try {
                while (($Read = $Stream.Read($Buffer, 0, $Buffer.Length)) -gt 0) {
                    [void]$Sha.TransformBlock($Buffer, 0, $Read, $Buffer, 0)
                    $Total += $Read
                }
            } finally {
                $Stream.Dispose()
            }
        }

        $Empty = New-Object byte[] 0
        [void]$Sha.TransformFinalBlock($Empty, 0, 0)
        return [pscustomobject]@{
            Size = $Total
            Sha256 = ConvertTo-Hex -Bytes $Sha.Hash
        }
    } finally {
        $Sha.Dispose()
    }
}

$CoreArchive = Get-Item -LiteralPath $CoreArchivePath
if ($CoreArchive.Name -cne $ExpectedCoreName) {
    throw "Core archive must be named exactly '$ExpectedCoreName'."
}

$PartCandidates = @(
    Get-ChildItem -LiteralPath $UcasPartsDirectory -File |
        Where-Object { $_.Name -like 'Rotorline-Windows.ucas.part*' }
)
if ($PartCandidates.Count -eq 0) {
    throw 'No UCAS parts were found.'
}

$NumberedParts = foreach ($Part in $PartCandidates) {
    if ($Part.Name -cnotmatch '^Rotorline-Windows\.ucas\.part([0-9]+)$') {
        throw "Invalid UCAS part name: $($Part.Name)"
    }
    [pscustomobject]@{
        Number = [int]$Matches[1]
        File = $Part
    }
}
$NumberedParts = @($NumberedParts | Sort-Object Number)
for ($Index = 0; $Index -lt $NumberedParts.Count; $Index++) {
    if ($NumberedParts[$Index].Number -ne ($Index + 1)) {
        throw 'UCAS parts must be contiguous from part1 without duplicates or gaps.'
    }
}
$Parts = @($NumberedParts | Select-Object -ExpandProperty File)

$Zip = [System.IO.Compression.ZipFile]::OpenRead($CoreArchive.FullName)
try {
    $EmbeddedUcas = @(
        $Zip.Entries | Where-Object {
            $_.FullName.Replace('\', '/') -eq $UcasRelativePath
        }
    )
    if ($EmbeddedUcas.Count -ne 0) {
        throw 'The core archive must not contain Rotorline-Windows.ucas.'
    }

    $InstalledFiles = @(
        foreach ($Pair in $ExpectedFiles.GetEnumerator()) {
            Get-ZipEntrySpec -Archive $Zip -Key $Pair.Key -RelativePath $Pair.Value
        }
    )
    [long]$CoreUncompressedBytes = (
        $Zip.Entries | Measure-Object -Property Length -Sum
    ).Sum
} finally {
    $Zip.Dispose()
}

$Assembled = Get-ConcatenatedHash -Parts $Parts
$UcasInstalled = [ordered]@{
    key = 'ucas'
    relativePath = $UcasRelativePath
    size = [long]$Assembled.Size
    sha256 = $Assembled.Sha256
}
$InstalledFiles = @($InstalledFiles) + @($UcasInstalled)

$PartSpecs = @($Parts | ForEach-Object { Get-FileSpec -File $_ })
[long]$DownloadBytes = $CoreArchive.Length + ($Parts | Measure-Object -Property Length -Sum).Sum
[long]$WorkingBytes = $DownloadBytes + $CoreUncompressedBytes + $Assembled.Size
[long]$RequiredFreeBytes = [long][Math]::Ceiling(
    ($WorkingBytes * 1.15) + 1GB
)

$Manifest = [ordered]@{
    schemaVersion = 1
    releaseTag = $ReleaseTag
    requiredFreeBytes = $RequiredFreeBytes
    coreArchive = Get-FileSpec -File $CoreArchive
    ucas = [ordered]@{
        relativePath = $UcasRelativePath
        assembledSize = [long]$Assembled.Size
        assembledSha256 = $Assembled.Sha256
        parts = $PartSpecs
    }
    installedFiles = $InstalledFiles
}

$ResolvedOutput = [System.IO.Path]::GetFullPath($OutputPath)
$OutputDirectory = Split-Path -Parent $ResolvedOutput
if (-not [string]::IsNullOrWhiteSpace($OutputDirectory)) {
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
}
$Json = $Manifest | ConvertTo-Json -Depth 8
[System.IO.File]::WriteAllText(
    $ResolvedOutput,
    $Json + [Environment]::NewLine,
    (New-Object System.Text.UTF8Encoding($false))
)

[pscustomobject]@{
    Manifest = $ResolvedOutput
    ReleaseTag = $ReleaseTag
    UcasParts = $Parts.Count
    DownloadBytes = $DownloadBytes
    AssembledUcasBytes = $Assembled.Size
    AssembledUcasSha256 = $Assembled.Sha256
    RequiredFreeBytes = $RequiredFreeBytes
}
