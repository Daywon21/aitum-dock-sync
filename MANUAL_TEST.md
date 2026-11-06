# Manual tests on Captain Dark's PC

## Chunk 4: plugin location move (OBS 33.0.1)

Installer: `installer\output\AitumDockSync-Setup-1.0.0.exe` (built by `scripts\Build-Installer.ps1`).

1. Close OBS completely (including the tray icon).
2. Run the installer (normal double-click, accept UAC).
3. Check files (PowerShell):
   ```powershell
   Test-Path "C:\ProgramData\obs-studio\plugins\aitum-dock-sync\aitum-dock-sync.dll"          # True
   Test-Path "C:\ProgramData\obs-studio\plugins\aitum-dock-sync\data\locale\en-US.ini"        # True
   Test-Path "C:\ProgramData\obs-studio\plugins\aitum-dock-sync\bin"                          # False
   Test-Path "C:\Program Files\obs-studio\obs-plugins\64bit\aitum-dock-sync.dll"              # False
   Test-Path "C:\Program Files\obs-studio\data\obs-plugins\aitum-dock-sync"                   # False
   Test-Path "$env:APPDATA\obs-studio\plugins\64bit\aitum-dock-sync.dll"                       # False
   ```
4. Start OBS. In the newest log (`%APPDATA%\obs-studio\logs`):
   - `[aitum-dock-sync] Loaded.`
   - `[aitum-dock-sync] Module: C:/ProgramData/obs-studio/plugins/aitum-dock-sync/aitum-dock-sync.dll, data: C:/ProgramData/obs-studio/plugins/aitum-dock-sync/data` (slashes may differ)
5. Tools > Plugin Manager: Aitum Dock Sync is listed and NOT marked Legacy.
6. Start streaming: Aitum Multistream outputs start (about 1.5 s after the main stream, 0.4 s apart).
   Stop streaming: they all stop.

Result: PASS / FAIL, with the log lines from step 4 and the output of step 3.

## Chunk 9: platform checkbox dock (OBS 33.0.1), rerun after Chunks 10 + 11

Installer: `installer\output\AitumDockSync-Setup-1.0.0.exe`
(sha256 3c9ddf12f224bad7ed30c0361f3f05a5386416d2e1bfa540b7e8f855a6f2ddfc, 2026-11-03 06:32).
Leave Aitum Vertical's "Start streaming with main" ON for this run.

1. Close OBS completely. Run the installer.
2. Start OBS. Docks menu: "Aitum Dock Sync". Dock it next to Aitum Multistream.
   - Group "Main": YouTube Horizontal, Kick, Facebook, X. Group "Vertical": both Vertical outputs.
   - Same font, spacing and colors as OBS's other docks. Check on Yami, Dark and Light (Settings > Appearance).
   - Log: `Dock outputs for profile '<p>': ...` and `Dock vertical outputs for profile '<p>': ...`
3. Tick some Main and Vertical outputs. Start streaming: only the ticked ones go live (about 1.5 s after the main
   stream, Main first, then Vertical). LIVE shows next to each output that is streaming, and only those.
   - Unticked Verticals that Aitum Vertical starts by itself are stopped: right away if already live, or
     the moment they connect (log: "is connecting; it will be stopped when it goes live.").
   - A Vertical you started by hand BEFORE going live keeps running and becomes ticked in the dock
     (log: "was already live when the stream started; ticked it.").
4. While live, untick one Main and one Vertical output: each stops right away and its LIVE goes off.
5. While live, add a platform in Aitum Multistream (Main or Vertical): it appears in the right group, unticked,
   not live. Tick it: it goes live and LIVE turns on.
6. Stop streaming: every Aitum output stops (Main and Vertical, ticked or not); all LIVE labels go off.
7. Restart OBS: the ticks (Main and Vertical) are still there for that profile.
   (Optional) Switch OBS profile: the dock shows that profile's outputs and ticks.
8. Search the newest OBS log for "stream_key" and for your stream keys: no matches from [aitum-dock-sync].

Result: PASS / FAIL per step, with the `[aitum-dock-sync]` lines from the OBS log.
