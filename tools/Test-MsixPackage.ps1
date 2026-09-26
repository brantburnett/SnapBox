[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$PackagePath,

    [string]$ExpectedPublisher,

    [switch]$RequireSignature
)

$ErrorActionPreference = 'Stop'

$packagePath = (Resolve-Path -LiteralPath $PackagePath).Path
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.Security.Cryptography.Pkcs

$archive = [System.IO.Compression.ZipFile]::OpenRead($packagePath)
try {
    $manifestEntry = $archive.GetEntry('AppxManifest.xml')
    if ($null -eq $manifestEntry) {
        throw "Package '$packagePath' does not contain AppxManifest.xml."
    }

    $reader = [System.IO.StreamReader]::new($manifestEntry.Open())
    try {
        [xml]$manifest = $reader.ReadToEnd()
    }
    finally {
        $reader.Dispose()
    }

    $namespaceManager = [System.Xml.XmlNamespaceManager]::new($manifest.NameTable)
    $namespaceManager.AddNamespace('appx', 'http://schemas.microsoft.com/appx/manifest/foundation/windows10')
    $identity = $manifest.SelectSingleNode('/appx:Package/appx:Identity', $namespaceManager)
    $resource = $manifest.SelectSingleNode('/appx:Package/appx:Resources/appx:Resource', $namespaceManager)
    if ($null -eq $identity) {
        throw "Package '$packagePath' does not contain an identity."
    }

    if ($null -eq $resource -or [string]::IsNullOrWhiteSpace($resource.Language) -or $resource.Language -eq 'x-generate') {
        throw "Package '$packagePath' has an invalid generated resource language."
    }

    if (-not [string]::IsNullOrWhiteSpace($ExpectedPublisher) -and
        -not [string]::Equals($identity.Publisher, $ExpectedPublisher, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Package identity publisher '$($identity.Publisher)' does not match expected publisher '$ExpectedPublisher'."
    }

    if ($RequireSignature) {
        $signatureEntry = $archive.GetEntry('AppxSignature.p7x')
        if ($null -eq $signatureEntry) {
            throw "Package '$packagePath' does not contain an AppxSignature.p7x signature."
        }

        $signatureStream = [System.IO.MemoryStream]::new()
        try {
            $entryStream = $signatureEntry.Open()
            try {
                $entryStream.CopyTo($signatureStream)
            }
            finally {
                $entryStream.Dispose()
            }

            $signedCms = [System.Security.Cryptography.Pkcs.SignedCms]::new()
            $signedCms.Decode($signatureStream.ToArray())
            $signer = $signedCms.SignerInfos[0].Certificate.Subject

            if (-not [string]::Equals($identity.Publisher, $signer, [System.StringComparison]::OrdinalIgnoreCase)) {
                throw "Package identity publisher '$($identity.Publisher)' does not match signing certificate subject '$signer'."
            }
        }
        finally {
            $signatureStream.Dispose()
        }
    }
}
finally {
    $archive.Dispose()
}
