# SnapView

<img src="images/SnapView-128.png" alt="SnapView logo" width="64" align="right" />

SnapView is a lightweight Windows screenshot utility. It runs in the
notification area, captures a region of the screen with a keyboard shortcut,
and keeps each capture ("snap") on screen in its own floating window so you can
reference, copy, save, or share it.

## Install

Get SnapView from the Microsoft Store:

[**SnapView on the Microsoft Store**](https://apps.microsoft.com/detail/9N5LVJ90BC5Q)

SnapView requires Windows 10 version 1809 or later, or Windows 11, on x64 or
ARM64. The Store installs updates automatically.

## Using SnapView

| Action | How |
| --- | --- |
| Take a snap | Press **Ctrl+PrtScr**, then drag to select a region. Press **Esc** to cancel. |
| Reopen the last closed snap | Press **Ctrl+Shift+PrtScr**. |
| Zoom a snap | Scroll the mouse wheel over it. Middle-click to reset the zoom. |
| Drag a snap into another app | Hold **Ctrl** and drag the snap. |
| Copy, save, Quicksave, or share | Right-click the snap. |
| Close all snaps, open Options, or exit | Right-click the SnapView icon in the notification area. |

Snaps can be saved as PNG, BMP, GIF, or JPEG. Quicksave writes the snap
straight to a folder without prompting, using the default snap type.

### Options

Open **Options** from the notification area icon. Changes are saved
immediately. You can set:

- How many closed snaps to keep in history, and clear that history
- Whether open snaps are hidden while you take a new snap
- Whether the frame and close button appear when the pointer is over a snap
- The default snap type and the Quicksave folder (your Pictures folder by
  default)
- Whether SnapView launches at startup

## Building from source

SnapView is a native C++ Win32 application with a WinUI 3 Options window,
packaged as MSIX. Building requires Visual Studio with the **Desktop
development with C++** workload, the `v145` toolset, the Windows SDK
10.0.26100.0 or later, and the MSIX packaging tools.

From an x64 Developer Command Prompt for Visual Studio at the repository root:

```bat
msbuild SnapView.slnx /restore /m /p:Configuration=Debug /p:Platform=x64
```

To debug, set `SnapViewPackage` as the startup project in Visual Studio; it
deploys and launches SnapView with package identity.

See [AGENTS.md](AGENTS.md) for the project layout, architecture, output paths,
ARM64 and bundle builds, and the Microsoft Store release workflow.

## License

SnapView is licensed under the [Apache License 2.0](LICENSE).
