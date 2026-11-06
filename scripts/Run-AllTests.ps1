#Requires -Version 5.1
# Full suite (PLAN.md rule 4): pytest (installer, needs Administrator) + C++ QtTest via ctest.
$Root = Split-Path -Parent $PSScriptRoot
Push-Location $Root
Write-Host "===== pytest ====="
py -3 -m pytest
$Py = $LASTEXITCODE
Write-Host "===== ctest ====="
powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root "scripts\Run-CppTests.ps1")
$Ct = $LASTEXITCODE
Pop-Location
Write-Host "===== SUMMARY: pytest exit $Py, ctest exit $Ct ====="
if ($Py -ne 0 -or $Ct -ne 0) { exit 1 }
