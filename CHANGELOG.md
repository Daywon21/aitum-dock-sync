# Changelog

All notable changes to Aitum Dock Sync are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [2.0.0] - 2026-11-03

### Added

- Dedicated Aitum Dock Sync dock with one checkbox per Aitum Multistream output for the current OBS profile.
- Only checked outputs start when streaming begins. Stopping the stream stops every output.
- Checking or unchecking an output while live starts or stops that output immediately. Offline, the choice is saved only.
- Checkbox state is stored per OBS profile and restored after restart. Switching profiles updates the dock to match.
- Aitum Vertical support with a separate Vertical group beside Main.
- LIVE label next to each output that is actively streaming.
- Automatic dock refresh when platforms are added or removed in Aitum Multistream. New outputs appear unchecked. Returning outputs keep their previous checkbox state.
- Automatic stop for unchecked Vertical outputs that Aitum Vertical starts on its own.
- Vertical outputs started manually before going live remain running and are marked checked in the dock.
- Installer support for OBS version detection and the correct plugin layout.

### Changed

- Dock rows use OBS native styling and the Audio Mixer font for Yami, Dark, and Light themes.
- Install path moved to `C:\ProgramData\obs-studio\plugins\aitum-dock-sync\`.
- OBS 33 and later use a flat plugin layout. Older OBS versions use `bin\64bit`.
- Portable OBS installs into the OBS `plugins` folder.
- Plugin load logs module and data paths.

### Fixed

- Compatible with OBS 34, which no longer loads plugins from `C:\Program Files\obs-studio\obs-plugins\64bit`.
- Plugin is no longer marked Legacy in Plugin Manager.
- Vertical outputs that are still connecting are not started twice.
- Stop requests sent while a Vertical output is connecting are applied correctly.

### Removed

- Installer removes previous copies from Program Files, `%APPDATA%`, and alternate ProgramData layouts. Other plugins are left unchanged.
- Uninstall removes the plugin ProgramData folder.

### Security

- Stream keys are never read or logged.

## [1.0.0] - 2026-07-08

### Added

- Initial release.
- Starts and stops Aitum Multistream main outputs with the OBS main stream by clicking Aitum dock buttons.
- Windows Setup installer and uninstall support.
