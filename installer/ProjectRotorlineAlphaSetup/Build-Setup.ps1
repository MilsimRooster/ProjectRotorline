[CmdletBinding()]
param(
    [string]$OutputDirectory = 'E:\games\ProjectRotorlineInstallerBuild',
    [switch]$SkipSelfTest
)

$ErrorActionPreference = 'Stop'
$ProjectDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$Compiler = 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$OutputExecutable = Join-Path $OutputDirectory 'ProjectRotorlineAlphaSetup.exe'

if (-not (Test-Path -LiteralPath $Compiler -PathType Leaf)) {
    throw "C# compiler not found: $Compiler"
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null

$SourceFiles = @(
    Get-ChildItem -LiteralPath $ProjectDirectory -Filter '*.cs' -File |
        Sort-Object Name |
        Select-Object -ExpandProperty FullName
)
if ($SourceFiles.Count -eq 0) {
    throw 'No installer C# sources were found.'
}

& $Compiler `
    /nologo `
    /target:winexe `
    /optimize+ `
    /win32icon:"$ProjectDirectory\Application.ico" `
    /out:"$OutputExecutable" `
    /reference:System.dll `
    /reference:System.Core.dll `
    /reference:System.Drawing.dll `
    /reference:System.IO.Compression.dll `
    /reference:System.IO.Compression.FileSystem.dll `
    /reference:System.Runtime.Serialization.dll `
    /reference:System.Windows.Forms.dll `
    $SourceFiles

if ($LASTEXITCODE -ne 0) {
    throw 'Project Rotorline Alpha setup build failed.'
}

if (-not (Test-Path -LiteralPath $OutputExecutable -PathType Leaf)) {
    throw "Setup compiler did not create: $OutputExecutable"
}

if (-not $SkipSelfTest) {
    $SelfTestResult = Join-Path (
        [System.IO.Path]::GetTempPath()
    ) ("ProjectRotorlineInstallerSelfTest-{0}.txt" -f [guid]::NewGuid().ToString('N'))

    try {
        $Process = Start-Process `
            -FilePath $OutputExecutable `
            -ArgumentList @('--self-test', ('"{0}"' -f $SelfTestResult)) `
            -WindowStyle Hidden `
            -Wait `
            -PassThru

        if ($Process.ExitCode -ne 0) {
            $Details = if (Test-Path -LiteralPath $SelfTestResult) {
                Get-Content -Raw -LiteralPath $SelfTestResult
            } else {
                'The self-test did not create a result file.'
            }
            throw "Installer self-test failed: $Details"
        }

        $Result = Get-Content -Raw -LiteralPath $SelfTestResult
        if (-not $Result.StartsWith('PASS:', [StringComparison]::Ordinal)) {
            throw "Installer self-test returned an unexpected result: $Result"
        }

        Write-Output $Result

        & (Join-Path $ProjectDirectory 'Test-ReleaseManifest.ps1')
    } finally {
        if (Test-Path -LiteralPath $SelfTestResult) {
            Remove-Item -LiteralPath $SelfTestResult -Force
        }
    }
}

Get-Item -LiteralPath $OutputExecutable |
    Select-Object FullName, Length, LastWriteTime
