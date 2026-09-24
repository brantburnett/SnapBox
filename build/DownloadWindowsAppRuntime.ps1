[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Uri,

    [Parameter(Mandatory = $true)]
    [string]$Destination,

    [Parameter(Mandatory = $true)]
    [string]$Sha256
)

$ErrorActionPreference = 'Stop'

function Get-Sha256 {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $stream = [System.IO.File]::OpenRead($Path)
        try {
            return ([BitConverter]::ToString($sha256.ComputeHash($stream))).Replace('-', '')
        }
        finally {
            $stream.Dispose()
        }
    }
    finally {
        $sha256.Dispose()
    }
}

function Test-Installer {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$ExpectedSha256
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $false
    }

    $actualSha256 = Get-Sha256 -Path $Path
    if (-not [string]::Equals($actualSha256, $ExpectedSha256, [StringComparison]::OrdinalIgnoreCase)) {
        return $false
    }

    $signTool = Get-ChildItem -Path "${env:ProgramFiles(x86)}\Windows Kits\10\bin" `
        -Filter signtool.exe `
        -Recurse `
        -ErrorAction SilentlyContinue |
        Where-Object { $_.Directory.Name -eq 'x64' } |
        Sort-Object FullName -Descending |
        Select-Object -First 1

    if (-not $signTool) {
        throw 'signtool.exe was not found. Install the Windows SDK signing tools to verify the runtime installer.'
    }

    $signatureOutput = & $signTool.FullName verify /pa /v $Path 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) {
        throw "Windows App Runtime installer signature validation failed:`n$signatureOutput"
    }

    if ($signatureOutput -notmatch 'Issued to: Microsoft Corporation') {
        throw "Windows App Runtime installer signer is not Microsoft Corporation:`n$signatureOutput"
    }

    return $true
}

if (Test-Installer -Path $Destination -ExpectedSha256 $Sha256) {
    exit 0
}

$destinationDirectory = Split-Path -Parent $Destination
New-Item -ItemType Directory -Force -Path $destinationDirectory | Out-Null

$temporaryPath = "$Destination.download"
Remove-Item -LiteralPath $temporaryPath -Force -ErrorAction SilentlyContinue

try {
    Invoke-WebRequest -Uri $Uri -OutFile $temporaryPath

    if (-not (Test-Installer -Path $temporaryPath -ExpectedSha256 $Sha256)) {
        throw 'Downloaded Windows App Runtime installer hash does not match the expected SHA-256 value.'
    }

    Move-Item -LiteralPath $temporaryPath -Destination $Destination -Force
}
finally {
    Remove-Item -LiteralPath $temporaryPath -Force -ErrorAction SilentlyContinue
}
