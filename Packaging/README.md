# MSIX packaging

`New-SnapBoxMsix.ps1` stages a Release application output directory into a
full-trust MSIX package. `New-SnapBoxMsixBundle.ps1` combines the x64 and ARM64
packages into one MSIX bundle. These are parallel artifacts; the WiX MSI
installers remain the supported installer distribution path.

The package identity is intentionally supplied at build time:

| Setting | GitHub configuration | Purpose |
|---|---|---|
| `PackageIdentityName` | `vars.MSIX_PACKAGE_IDENTITY_NAME` | Stable package name. |
| `PackagePublisher` | `vars.MSIX_PACKAGE_PUBLISHER` | Must exactly match the Azure Artifact Signing certificate subject. |
| `PackagePublisherDisplayName` | `vars.MSIX_PACKAGE_PUBLISHER_DISPLAY_NAME` | Displayed publisher name. |

Pull-request builds use the fixed temporary `BurnettSoft.SnapBox` /
`CN=BurnettSoft` identity because their MSIX packages are unsigned validation
artifacts. Main and tag release builds require the configured values above so
their signed bundles have the intended identity.

The current manifest uses the Windows App Runtime `Microsoft.WindowsAppRuntime.2`
framework dependency at version `2.5.1.0`, matching the
`Microsoft.WindowsAppSDK` NuGet package version in `SnapBox.vcxproj`.

For a local package, build the desired Release architecture, then run:

```powershell
.\tools\New-SnapBoxMsix.ps1 `
  -Platform x64 `
  -ApplicationDirectory .\x64\Release `
  -OutputDirectory .\artifacts\msix `
  -Version 0.2.0 `
  -PackageIdentityName '<identity name>' `
  -PackagePublisher '<certificate subject>'
```

Package both architectures, bundle them, then install the signed bundle with
`Add-AppxPackage`. The signing certificate must be trusted on the test device.
Because this is framework-dependent, sideloading also requires the matching
Microsoft Windows App Runtime framework package for the device architecture;
the Microsoft Store resolves that dependency automatically. Before registering
a Microsoft Store package, reserve SnapBox in Partner Center and replace the
temporary identity values with the exact Store package name and publisher.
