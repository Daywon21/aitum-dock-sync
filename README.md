# Aitum Dock Sync

OBS Studio plugin that starts and stops [Aitum Multistream](https://aitum.tv/products/multi) outputs automatically with the OBS main stream.

When you click Start Streaming, the plugin clicks the Aitum Multistream dock buttons for your configured outputs. When you stop streaming, it clicks stop. The Aitum dock stays in sync, including button state.

## Requirements

- OBS Studio 64-bit (Windows)
- [Aitum Multistream](https://aitum.tv/products/multi) installed and configured

## Install

1. Download `AitumDockSync-Setup-1.0.0.exe` from the [latest release](https://github.com/Daywon21/aitum-dock-sync/releases/latest)
2. Close OBS Studio completely
3. Run the setup
4. Restart OBS Studio
5. Open **Tools → Scripts** and remove `aitum_sync.py` if it is still listed

## Uninstall

1. Close OBS Studio completely
2. Open **Settings → Apps → Installed apps**
3. Select **Aitum Dock Sync** and choose Uninstall  
   Or use **Start → Aitum Dock Sync → Uninstall Aitum Dock Sync**
4. Restart OBS Studio

## Usage

1. Configure your secondary outputs in the Aitum Multistream dock
2. Start streaming in OBS
3. After a short delay, Aitum outputs start automatically
4. Stop streaming in OBS to stop those outputs

## Build from source

### Prerequisites

- Visual Studio 2022 with C++ tools
- CMake and Ninja
- Qt 6 (MSVC 2022 64-bit)
- OBS Studio development headers and import libraries
- [Inno Setup 6](https://jrsoftware.org/isinfo.php) for the Windows installer

### Plugin only

```powershell
cd plugin
.\Build.ps1
```

Environment overrides:

- `QT_DIR`
- `OBS_LIBOBS_DIR`
- `OBS_FRONTEND_DIR`
- `OBS_LIB_DIR`

### Windows installer

```powershell
.\scripts\Build-Installer.ps1
```

Output:

`installer\output\AitumDockSync-Setup-<version>.exe`

## License

GPL-2.0
