[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateSet('x64', 'ARM64')]
    [string]$Platform,

    [Parameter(Mandatory)]
    [string]$ApplicationDirectory,

    [Parameter(Mandatory)]
    [string]$OutputDirectory,

    [Parameter(Mandatory)]
    [string]$Version,

    [Parameter(Mandatory)]
    [string]$PackageIdentityName,

    [Parameter(Mandatory)]
    [string]$PackagePublisher,

    [string]$PackageDisplayName = 'SnapBox',

    [string]$PackagePublisherDisplayName = 'BurnettSoft',

    [string]$WindowsAppRuntimePackageName = 'Microsoft.WindowsAppRuntime.2',

    [string]$WindowsAppRuntimePackagePublisher = 'CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US',

    [string]$WindowsAppRuntimePackageVersion = '2.5.1.0',

    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'

function Get-MsixVersion([string]$semanticVersion) {
    $match = [System.Text.RegularExpressions.Regex]::Match(
        $semanticVersion,
        '^(?<major>\d+)\.(?<minor>\d+)\.(?<build>\d+)(?:\.(?<revision>\d+))?(?:[-+].*)?$')

    if (-not $match.Success) {
        throw "Version '$semanticVersion' must begin with major.minor.patch for MSIX packaging."
    }

    $parts = @(
        $match.Groups['major'].Value,
        $match.Groups['minor'].Value,
        $match.Groups['build'].Value,
        $(if ($match.Groups['revision'].Success) { $match.Groups['revision'].Value } else { '0' }))

    foreach ($part in $parts) {
        if ([uint32]$part -gt 65535) {
            throw "MSIX version component '$part' in '$semanticVersion' exceeds 65535."
        }
    }

    return $parts -join '.'
}

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

function Escape-Xml([string]$value) {
    return [System.Security.SecurityElement]::Escape($value)
}

$applicationDirectory = [System.IO.Path]::GetFullPath($ApplicationDirectory)
$outputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
$repositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)

if (-not (Test-Path -LiteralPath $applicationDirectory -PathType Container)) {
    throw "Application directory '$applicationDirectory' does not exist."
}

$executablePath = Join-Path $applicationDirectory 'SnapBox.exe'
if (-not (Test-Path -LiteralPath $executablePath -PathType Leaf)) {
    throw "Expected executable '$executablePath' was not found."
}

$manifestTemplatePath = Join-Path $repositoryRoot 'Packaging\AppxManifest.xml.template'
if (-not (Test-Path -LiteralPath $manifestTemplatePath -PathType Leaf)) {
    throw "MSIX manifest template '$manifestTemplatePath' was not found."
}

$requiredAssets = @(
    'StoreLogo.png',
    'Square44x44Logo.png',
    'Square71x71Logo.png',
    'Square150x150Logo.png',
    'Square310x310Logo.png',
    'Wide310x150Logo.png'
)
$imagesDirectory = Join-Path $repositoryRoot 'images'
foreach ($asset in $requiredAssets) {
    if (-not (Test-Path -LiteralPath (Join-Path $imagesDirectory $asset) -PathType Leaf)) {
        throw "Required MSIX asset '$asset' was not found in '$imagesDirectory'."
    }
}

$msixVersion = Get-MsixVersion $Version
$architecture = if ($Platform -eq 'ARM64') { 'arm64' } else { 'x64' }
$safeVersion = $Version -replace '[^0-9A-Za-z.-]', '-'
$packageFileName = "SnapBox-$safeVersion-$architecture.msix"
$packagePath = Join-Path $outputDirectory $packageFileName
$stagingDirectory = Join-Path ([System.IO.Path]::GetTempPath()) "SnapBox-Msix-$architecture-$([guid]::NewGuid())"

New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $stagingDirectory -Force | Out-Null

try {
    $excludedBuildArtifacts = @('*.exp', '*.ilk', '*.lib', '*.pdb')
    Get-ChildItem -LiteralPath $applicationDirectory -Force |
        Where-Object {
            $name = $_.Name
            -not ($excludedBuildArtifacts | Where-Object { $name -like $_ })
        } |
        Copy-Item -Destination $stagingDirectory -Recurse -Force

    $assetsDirectory = Join-Path $stagingDirectory 'Assets'
    New-Item -ItemType Directory -Path $assetsDirectory -Force | Out-Null
    foreach ($asset in $requiredAssets) {
        Copy-Item -LiteralPath (Join-Path $imagesDirectory $asset) -Destination $assetsDirectory -Force
    }

    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'Distribution\license.rtf') -Destination $stagingDirectory -Force

    $manifest = Get-Content -LiteralPath $manifestTemplatePath -Raw
    $replacements = @{
        '__PACKAGE_IDENTITY_NAME__' = Escape-Xml $PackageIdentityName
        '__PACKAGE_PUBLISHER__' = Escape-Xml $PackagePublisher
        '__PACKAGE_VERSION__' = $msixVersion
        '__PACKAGE_ARCHITECTURE__' = $architecture
        '__PACKAGE_DISPLAY_NAME__' = Escape-Xml $PackageDisplayName
        '__PACKAGE_PUBLISHER_DISPLAY_NAME__' = Escape-Xml $PackagePublisherDisplayName
        '__WINDOWS_APP_RUNTIME_PACKAGE_NAME__' = Escape-Xml $WindowsAppRuntimePackageName
        '__WINDOWS_APP_RUNTIME_PACKAGE_PUBLISHER__' = Escape-Xml $WindowsAppRuntimePackagePublisher
        '__WINDOWS_APP_RUNTIME_PACKAGE_VERSION__' = $WindowsAppRuntimePackageVersion
    }
    foreach ($replacement in $replacements.GetEnumerator()) {
        $manifest = $manifest.Replace($replacement.Key, $replacement.Value)
    }
    [System.IO.File]::WriteAllText(
        (Join-Path $stagingDirectory 'AppxManifest.xml'),
        $manifest,
        [System.Text.UTF8Encoding]::new($false))

    $makeAppx = Get-MakeAppxPath
    if (Test-Path -LiteralPath $packagePath) {
        Remove-Item -LiteralPath $packagePath -Force
    }

    & $makeAppx pack /d $stagingDirectory /p $packagePath /o
    if ($LASTEXITCODE -ne 0) {
        throw "MakeAppx.exe failed while creating '$packagePath'."
    }

    Write-Output $packagePath
}
finally {
    Remove-Item -LiteralPath $stagingDirectory -Recurse -Force -ErrorAction SilentlyContinue
}
