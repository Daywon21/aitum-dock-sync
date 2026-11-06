#Requires -Version 5.1
# Read-only tools check (BUILD_PROMPT "Tools check"). Installs nothing, changes nothing.
$ErrorActionPreference = "Continue"
$Desktop = Join-Path $env:USERPROFILE "Desktop"
$Vcvars  = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$Cmc     = Join-Path $Desktop "captain-multi-chat"
$Checks = [ordered]@{
  "vcvars64.bat"               = $Vcvars
  "Qt 6.8.3 msvc2022_64"       = Join-Path $Cmc ".deps\qt\6.8.3\msvc2022_64"
  "Qt6Test.dll"                = Join-Path $Cmc ".deps\qt\6.8.3\msvc2022_64\bin\Qt6Test.dll"
  "libobs headers"             = Join-Path $Cmc ".deps\obs-studio-32.1.2\libobs\obs-module.h"
  "frontend api header"        = Join-Path $Cmc ".deps\obs-studio-32.1.2\frontend\api\obs-frontend-api.h"
  "obs.lib"                    = Join-Path $Cmc ".deps\obs-dev\lib\obs.lib"
  "obs-frontend-api.lib"       = Join-Path $Cmc ".deps\obs-dev\lib\obs-frontend-api.lib"
}
foreach ($k in $Checks.Keys) { "{0,-24} {1}  {2}" -f $k, $(if (Test-Path $Checks[$k]) {"OK     "} else {"MISSING"}), $Checks[$k] }

$Iscc = @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe","$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe","${env:ProgramFiles}\Inno Setup 6\ISCC.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
"{0,-24} {1}  {2}" -f "Inno Setup ISCC", $(if ($Iscc) {"OK     "} else {"MISSING"}), $Iscc

$obs = "C:\Program Files\obs-studio\bin\64bit\obs64.exe"
if (Test-Path $obs) { "{0,-24} {1}" -f "obs64.exe version", (Get-Item $obs).VersionInfo.FileVersion } else { "obs64.exe                MISSING  $obs" }

"--- cmake / ninja / cl (inside vcvars64) ---"
cmd.exe /c "call `"$Vcvars`" >nul && cmake --version && ninja --version && cl 2>&1 | findstr /C:Version"
"--- python / pytest ---"
foreach ($c in @("py -3 --version","py -3 -m pytest --version","python --version","python -m pytest --version")) { "> $c"; cmd.exe /c "$c 2>&1" }
