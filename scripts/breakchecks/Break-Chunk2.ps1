#Requires -Version 5.1
# Break checks for Chunk 2 (PLAN.md rule 8). Run from an Administrator PowerShell.
# Each break edits installer\aitum-dock-sync.iss, runs the Chunk 2 tests (the named
# ones must FAIL), then restores the .iss and confirms it is byte-identical.
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Target = Join-Path $Root "installer\aitum-dock-sync.iss"
$Backup = "$Target.breakcheck-bak"

$Breaks = [ordered]@{
  "A: flat cut-off moved to OBS 35 (expect obs33 + obs34 FAIL)" = @("(ObsMajor < 33)", "(ObsMajor < 35)")
  "B: flat cut-off moved to OBS 32 (expect obs32 FAIL)"         = @("(ObsMajor < 33)", "(ObsMajor < 32)")
  "C: portable detection off (expect portable FAIL)"           = @("ObsPortable := IsPortableObs(ObsDir);", "ObsPortable := False;")
  "D: legacy-cleanup BEGIN marker removed (expect no_hardcoded FAIL)" = @("    // ; BEGIN legacy-cleanup`r`n", "")
}

$HashBefore = (Get-FileHash $Target).Hash
foreach ($Name in $Breaks.Keys) {
  $Good, $Bad = $Breaks[$Name]
  Copy-Item -Force $Target $Backup
  try {
    $Text = [IO.File]::ReadAllText($Target)
    $GoodN = $Good.Replace("`r`n", "`n")
    if ($Text.Contains($Good)) { $Text = $Text.Replace($Good, $Bad) }
    elseif ($Text.Contains($GoodN)) { $Text = $Text.Replace($GoodN, $Bad) }
    else { throw "Break point not found: $Name" }
    [IO.File]::WriteAllText($Target, $Text)
    Write-Host "=== BROKEN $Name ==="
    Push-Location $Root
    py -3 -m pytest tests/test_chunk2_installer.py
    Pop-Location
  } finally {
    Copy-Item -Force $Backup $Target
    Remove-Item -Force $Backup
    Write-Host "=== RESTORED after $Name : hash match = $($HashBefore -eq (Get-FileHash $Target).Hash) ==="
  }
}
