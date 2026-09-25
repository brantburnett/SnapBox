[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$PackageDirectory,

    [Parameter(Mandatory)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

function Get-MakeAppxPath {
    $kitsRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
    $candidates = @(
        Get-ChildItem -Path $kitsRoot -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } |
            Sort-Object { [version]$_.Name } -Descending |
            ForEach-Object { Join-Path $_.FullName 'x64\makeappx.exe' } |
            Where-Object { Test-Path -LiteralPath $_ }
    )

    if ($candidates.Count -eq 0) {
        throw "MakeAppx.exe was not found under '$kitsRoot'. Install a Windows 10 or Windows 11 SDK."
    }

    return $candidates[0]
}

$packageDirectory = [System.IO.Path]::GetFullPath($PackageDirectory)
$outputPath = [System.IO.Path]::GetFullPath($OutputPath)

if (-not (Test-Path -LiteralPath $packageDirectory -PathType Container)) {
    throw "MSIX package directory '$packageDirectory' does not exist."
}

$packages = @(Get-ChildItem -LiteralPath $packageDirectory -Filter '*.msix' -File)
if ($packages.Count -lt 2) {
    throw "Expected at least two architecture-specific MSIX packages in '$packageDirectory'."
}

$outputDirectory = Split-Path -Parent $outputPath
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
if (Test-Path -LiteralPath $outputPath) {
    Remove-Item -LiteralPath $outputPath -Force
}

$makeAppx = Get-MakeAppxPath
& $makeAppx bundle /d $packageDirectory /p $outputPath /o
if ($LASTEXITCODE -ne 0) {
    throw "MakeAppx.exe failed while creating '$outputPath'."
}

Write-Output $outputPath
