#Requires -Version 5.1
$ErrorActionPreference = "Stop"

$PluginRoot = $PSScriptRoot
$Build = Join-Path $PluginRoot "build"
$RepoRoot = Split-Path -Parent $PluginRoot

$Vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $Vcvars)) {
    $Vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
}
if (-not (Test-Path $Vcvars)) {
    throw "MSVC vcvars64.bat was not found. Install Visual Studio Build Tools with the C++ workload."
}

$QtDir = $env:QT_DIR
if (-not $QtDir) {
    $candidates = @(
        (Join-Path $RepoRoot ".deps\qt\6.8.3\msvc2022_64"),
        (Join-Path $RepoRoot "..\captain-multi-chat\.deps\qt\6.8.3\msvc2022_64")
    )
    $QtDir = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $QtDir) {
    throw "Qt was not found. Set QT_DIR to your Qt 6 msvc2022_64 directory."
}

$ObsLibobs = $env:OBS_LIBOBS_DIR
if (-not $ObsLibobs) {
    $ObsLibobs = Join-Path $RepoRoot "..\captain-multi-chat\.deps\obs-studio-32.1.2\libobs"
}
$ObsFrontend = $env:OBS_FRONTEND_DIR
if (-not $ObsFrontend) {
    $ObsFrontend = Join-Path $RepoRoot "..\captain-multi-chat\.deps\obs-studio-32.1.2\frontend\api"
}
$ObsLib = $env:OBS_LIB_DIR
if (-not $ObsLib) {
    $ObsLib = Join-Path $RepoRoot "..\captain-multi-chat\.deps\obs-dev\lib"
}

if (Test-Path $Build) {
    Remove-Item -Recurse -Force $Build -ErrorAction SilentlyContinue
}

$Configure = @(
    "call `"$Vcvars`" && cmake -S `"$PluginRoot`" -B `"$Build`" -G Ninja",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_PREFIX_PATH=`"$QtDir`"",
    "-DOBS_LIBOBS_DIR=`"$ObsLibobs`"",
    "-DOBS_FRONTEND_DIR=`"$ObsFrontend`"",
    "-DOBS_LIB_DIR=`"$ObsLib`""
) -join " "

cmd.exe /c $Configure
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed."
}

$Compile = "call `"$Vcvars`" && cmake --build `"$Build`" --parallel"
cmd.exe /c $Compile
if ($LASTEXITCODE -ne 0) {
    throw "Build failed."
}

Write-Host "Built $(Join-Path $Build 'aitum-dock-sync.dll')" -ForegroundColor Green
