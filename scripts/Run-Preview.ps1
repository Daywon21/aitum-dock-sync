#Requires -Version 5.1
# Builds the tests (incl. the dock_preview tool) and renders dock previews to release\preview.
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Build = Join-Path $Root "release\test-build"
$Out = Join-Path $Root "release\preview"
$QtDir = "c:\Users\CaptainDark\Desktop\captain-multi-chat\.deps\qt\6.8.3\msvc2022_64"
powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root "scripts\Run-CppTests.ps1") | Out-Null
$env:PATH = "$QtDir\bin;$env:PATH"
$env:QT_PLUGIN_PATH = "$QtDir\plugins"
& (Join-Path $Build "tests\dock_preview.exe") $Out
Get-ChildItem $Out -Filter *.png | Select-Object Name, Length
