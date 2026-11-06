#Requires -Version 5.1
$ErrorActionPreference = "Stop"

$PluginRoot = $PSScriptRoot
$Build = Join-Path $PluginRoot "build"
$Vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$QtDir = "c:\Users\CaptainDark\Desktop\captain-multi-chat\.deps\qt\6.8.3\msvc2022_64"

if (-not (Test-Path $Vcvars)) {
    throw "MSVC environment script not found: $Vcvars"
}
if (-not (Test-Path $QtDir)) {
    throw "Qt installation not found: $QtDir"
}

if (Test-Path $Build) {
    Remove-Item -Recurse -Force $Build -ErrorAction SilentlyContinue
}

$Configure = "call `"$Vcvars`" && cmake -S `"$PluginRoot`" -B `"$Build`" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=`"$QtDir`""
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
