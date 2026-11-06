#Requires -Version 5.1
$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$PluginRoot = Join-Path $Root "plugin"
$PackageRoot = Join-Path $Root "release\Package"
$InstallerDir = Join-Path $Root "installer"
$Iss = Join-Path $InstallerDir "aitum-dock-sync.iss"
$VersionFile = Join-Path $Root "VERSION"
$Version = if (Test-Path $VersionFile) { (Get-Content $VersionFile -Raw).Trim() } else { "2.0.0" }

$IsccCandidates = @(
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles}\Inno Setup 6\ISCC.exe"
)
$Iscc = $IsccCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $Iscc) {
    throw "Inno Setup 6 was not found. Install it from https://jrsoftware.org/isinfo.php"
}

Write-Host "Building plugin..."
& (Join-Path $PluginRoot "Build.ps1")
if (-not (Test-Path (Join-Path $PluginRoot "build\aitum-dock-sync.dll"))) {
    throw "Plugin build failed."
}

Write-Host "Staging package..."
if (Test-Path $PackageRoot) {
    Remove-Item -Recurse -Force $PackageRoot
}
$PluginOut = Join-Path $PackageRoot "aitum-dock-sync"
$DataOut = Join-Path $PluginOut "data"
New-Item -ItemType Directory -Force -Path $PluginOut | Out-Null
Copy-Item -Force (Join-Path $PluginRoot "build\aitum-dock-sync.dll") (Join-Path $PluginOut "aitum-dock-sync.dll")
Copy-Item -Recurse -Force (Join-Path $PluginRoot "data") $DataOut

Write-Host "Compiling installer..."
$OutputDir = Join-Path $InstallerDir "output"
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
& $Iscc "/DMyAppVersion=$Version" "/DPackageRoot=$PackageRoot" $Iss
if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup compile failed."
}

$Setup = Join-Path $OutputDir "AitumDockSync-Setup-$Version.exe"
if (-not (Test-Path $Setup)) {
    throw "Installer output missing: $Setup"
}

Write-Host ""
Write-Host "Installer ready: $Setup" -ForegroundColor Green
Write-Host "Users run that Setup.exe. No Install.cmd needed."
