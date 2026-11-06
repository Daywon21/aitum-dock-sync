#Requires -Version 5.1
param([string]$SpecFile = "chunk10-11-breaks.json", [switch]$NoInstaller)
# Break checks for Chunks 10 + 11 (PLAN.md rule 8), steps in chunk10-11-breaks.json.
# PRE/GREEN steps are the fail-first runs for the two source guards (they passed on the old
# code). Every edit is restored and hash-checked. Then the new installer is built.
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Spec = Get-Content -Raw (Join-Path $PSScriptRoot $SpecFile) | ConvertFrom-Json
$Run = Join-Path $Root "scripts\Run-CppTests.ps1"

foreach ($Step in $Spec.steps) {
  $Files = @($Step.edits | ForEach-Object { Join-Path $Root $_[0] } | Select-Object -Unique)
  $Hashes = @{}
  foreach ($F in $Files) { $Hashes[$F] = (Get-FileHash $F).Hash; Copy-Item -Force $F "$F.breakcheck-bak" }
  try {
    foreach ($E in $Step.edits) {
      $F = Join-Path $Root $E[0]
      $Text = [IO.File]::ReadAllText($F)
      if (-not $Text.Contains($E[1])) { throw "Edit point not found in $($Step.name)" }
      [IO.File]::WriteAllText($F, $Text.Replace($E[1], $E[2]))
    }
    Write-Host "=== $($Step.name) ==="
    if ($Step.run -like "py:*") {
      Push-Location $Root; py -3 -m pytest $Step.run.Substring(3); Pop-Location
    } else {
      powershell -NoProfile -ExecutionPolicy Bypass -File $Run -Filter $Spec.ctest_filter
    }
  } finally {
    foreach ($F in $Files) {
      Copy-Item -Force "$F.breakcheck-bak" $F
      Remove-Item -Force "$F.breakcheck-bak"
      Write-Host "=== RESTORED $([IO.Path]::GetFileName($F)) after $($Step.name) : hash match = $($Hashes[$F] -eq (Get-FileHash $F).Hash) ==="
    }
  }
}

if ($NoInstaller) { exit 0 }
Write-Host "=== INSTALLER ==="
powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root "scripts\Build-Installer.ps1")
$Setup = Join-Path $Root "installer\output\AitumDockSync-Setup-1.0.0.exe"
Get-Item $Setup | Select-Object FullName, Length, LastWriteTime
(Get-FileHash $Setup -Algorithm SHA256).Hash.ToLower()
