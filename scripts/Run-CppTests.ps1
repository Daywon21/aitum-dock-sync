#Requires -Version 5.1
# Builds the plugin with its QtTest unit tests and runs them with ctest.
# Optional: -Filter <regex> passes -R to ctest.
param([string]$Filter = "")
$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$PluginRoot = Join-Path $Root "plugin"
$Build = Join-Path $Root "release\test-build"
$Vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$QtDir = "c:\Users\CaptainDark\Desktop\captain-multi-chat\.deps\qt\6.8.3\msvc2022_64"

if (Test-Path $Build) { Remove-Item -Recurse -Force $Build }

$Configure = "call `"$Vcvars`" >nul && cmake -S `"$PluginRoot`" -B `"$Build`" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=`"$QtDir`" -DADS_BUILD_TESTS=ON"
cmd.exe /c $Configure
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

cmd.exe /c "call `"$Vcvars`" >nul && cmake --build `"$Build`" --parallel"
if ($LASTEXITCODE -ne 0) { throw "Build failed." }

$Filt = ""
if ($Filter) { $Filt = "-R `"$Filter`"" }
cmd.exe /c "call `"$Vcvars`" >nul && set `"PATH=$QtDir\bin;%PATH%`" && ctest --test-dir `"$Build`" --output-on-failure -V $Filt"
exit $LASTEXITCODE
