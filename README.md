# Aitum Dock Sync

OBS Studio plugin that starts and stops [Aitum Multistream](https://aitum.tv/products/multi) outputs with the OBS main stream.

Version 2.0 adds a dedicated dock so you choose which outputs sync. Only checked outputs start when you go live. Stop Streaming still stops every output.

## Requirements

- OBS Studio 64-bit (Windows)
- [Aitum Multistream](https://aitum.tv/products/multi) installed and configured

## Install

1. Download `AitumDockSync-Setup-2.0.0.exe` from the [latest release](https://github.com/Daywon21/aitum-dock-sync/releases/latest)
2. Close OBS Studio completely
3. Run the setup
4. Restart OBS Studio
5. Open **View → Docks → Aitum Dock Sync** and check the outputs you want

## Uninstall

1. Close OBS Studio completely
2. Open **Settings → Apps → Installed apps**
3. Select **Aitum Dock Sync** and choose Uninstall
4. Restart OBS Studio

## Where it installs

| OBS version | Module | Data |
|---|---|---|
| Older than 33 | `C:\ProgramData\obs-studio\plugins\aitum-dock-sync\bin\64bit\aitum-dock-sync.dll` | `C:\ProgramData\obs-studio\plugins\aitum-dock-sync\data\` |
| 33 and later | `C:\ProgramData\obs-studio\plugins\aitum-dock-sync\aitum-dock-sync.dll` | `C:\ProgramData\obs-studio\plugins\aitum-dock-sync\data\` |
| Portable OBS | `<obs>\plugins\aitum-dock-sync\aitum-dock-sync.dll` | `<obs>\plugins\aitum-dock-sync\data\` |

The installer reads the OBS version from `obs64.exe`. After upgrading OBS to 33 or later, run the installer again so the layout matches.

## OBS 34

From OBS Studio 34.0, plugins under `C:\Program Files\obs-studio\obs-plugins\64bit\` are no longer loaded. This plugin installs under ProgramData instead. The installer removes the old Program Files copy on every install.

## Usage

1. Configure platforms in the Aitum Multistream dock
2. Open the Aitum Dock Sync dock and check the outputs to sync
3. Start Streaming in OBS
4. Checked outputs start automatically
5. Stop Streaming to stop all Aitum outputs

## Build from source

### Prerequisites

- Visual Studio 2022 with C++ tools
- CMake and Ninja
- Qt 6 (MSVC 2022 64-bit)
- OBS Studio development headers and import libraries
- [Inno Setup 6](https://jrsoftware.org/isinfo.php) for the Windows installer

### Plugin

```powershell
cd plugin
.\Build.ps1
```

### Installer

```powershell
.\scripts\Build-Installer.ps1
```

Output:

`installer\output\AitumDockSync-Setup-<version>.exe`

## Changelog

See [CHANGELOG.md](CHANGELOG.md).

## License

GPL-2.0
