# SnapBox Agent Guide

## Repository overview

SnapBox is a native Windows desktop screenshot utility. The application is a
Unicode Win32 C++ project; its installer is a WiX project. The solution is
`SnapBox.slnx`, with these projects:

- `SnapBox\SnapBox.vcxproj`: application (`Debug|x64`, `Release|x64`,
  `Debug|ARM64`, and `Release|ARM64`)
- `SnapBoxInstall\SnapBoxInstall.wixproj`: MSI installer, built as part of the
  solution

The native project uses the `v145` toolset, the Windows App SDK, and
manifest-mode vcpkg. Its only vcpkg dependency is Xerces-C, installed using
the `x64-windows-static` or `arm64-windows-static` triplet in
`vcpkg_installed\`.

SnapBox is an unpackaged, framework-dependent Windows App SDK application.
The SDK bootstrapper initializes before the application's entry point. If the
matching runtime is missing, Windows displays acquisition UI; the MSI does not
bundle or silently install the runtime.

## Prerequisites

- Windows 10 version 1809 or later, or Windows 11
- Visual Studio with the **Desktop development with C++** workload, the
  `v145` toolset, and a Windows 10/11 SDK
- vcpkg; use the copy supplied with Visual Studio or a separately bootstrapped
  vcpkg executable
- .NET SDK (required by the WiX Toolset SDK project)
- Network access on the first restore to download vcpkg and NuGet packages

Do not check in `vcpkg_installed\`, build output, or Visual Studio user files;
they are intentionally ignored.

## Restore and build

Run the following from an **x64 Developer Command Prompt for Visual Studio** at
the repository root. Restore the vcpkg manifest before building; it creates the
project-local `vcpkg_installed\` directory using the pinned baseline in
`vcpkg-configuration.json`.

```bat
vcpkg install --triplet x64-windows-static
```

For ARM64 builds, install the ARM64 triplet instead:

```bat
vcpkg install --triplet arm64-windows-static
```

If `vcpkg` is not on `PATH`, Visual Studio installs it at a path similar to
`C:\Program Files\Microsoft Visual Studio\<version>\<edition>\VC\vcpkg\vcpkg.exe`.
Then build the solution. `/restore` restores the WiX NuGet packages.

```bat
msbuild SnapBox.slnx /restore /m /p:Configuration=Debug /p:Platform=x64
```

For a release build:

```bat
msbuild SnapBox.slnx /restore /m /p:Configuration=Release /p:Platform=x64
```

For an ARM64 release build:

```bat
msbuild SnapBox.slnx /restore /m /p:Configuration=Release /p:Platform=ARM64
```

To build only the executable, which avoids packaging the MSI:

```bat
msbuild SnapBox\SnapBox.vcxproj /restore /m /p:Configuration=Debug /p:Platform=x64
```

The application is emitted to
`<Platform>\<Configuration>\SnapBox.exe` (for example,
`ARM64\Release\SnapBox.exe`). The solution build also produces an MSI under
`SnapBoxInstall\bin\<Platform>\<Configuration>\`, with the platform in its
name (for example, `SnapBox-<version>-arm64.msi`). The Windows App SDK NuGet
package is restored by MSBuild's `/restore` switch; run the executable on a
machine without its matching runtime to validate the Windows-provided
acquisition UI.

There is no automated test project or test runner in this repository. Validate
native changes by building the affected configuration, and manually exercise
the Windows UI when changes affect capture behavior, options, hotkeys, email,
or installer content.

## Implementation guidance

- Keep the application x64 and ARM64 settings aligned when updating project,
  dependency, or installer behavior.
- Add C++ source, headers, resources, and images to
  `SnapBox\SnapBox.vcxproj` and keep `SnapBox\SnapBox.vcxproj.filters` in
  sync for Visual Studio users.
- `stdafx.cpp` creates the precompiled header. Files using shared Windows or
  Xerces headers should include `stdafx.h` first.
- Preserve the project runtime-library selection: `/MTd` for Debug and `/MT`
  for Release. New native dependencies must be compatible with the static
  vcpkg triplet.
- Update `SnapBoxInstall\Product.wxs` when installable files, product metadata,
  or installer behavior changes. Do not hand-edit generated build output.
- Use Unicode Win32 APIs and project conventions (`TCHAR`, `wstring`, and
  resource identifiers) when modifying existing UI code.
