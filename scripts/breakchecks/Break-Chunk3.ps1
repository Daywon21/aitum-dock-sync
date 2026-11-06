#Requires -Version 5.1
# Break checks for Chunk 3 (PLAN.md rule 8). Run from an Administrator PowerShell, OBS closed.
# Each break applies one or more edits to installer\aitum-dock-sync.iss, runs the Chunk 3
# tests (the named ones must FAIL), then restores the .iss and confirms byte-identical.
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Target = Join-Path $Root "installer\aitum-dock-sync.iss"
$Backup = "$Target.breakcheck-bak"
$N = "`n"

$Breaks = [ordered]@{
  "A: no Program Files DLL cleanup (expect legacy_program_files + other_plugins FAIL)" = @(
    ,@("  if not RemovePluginFile(GetLegacyModuleFile('')) then$N    Failed := Failed + GetLegacyModuleFile('') + #13#10;$N", ""))
  "B: no APPDATA cleanup (expect appdata + other_plugins FAIL)" = @(
    ,@("  if not RemovePluginFile(GetLegacyAppDataFile('')) then$N    Failed := Failed + GetLegacyAppDataFile('') + #13#10;$N", ""))
  "C: no other-shape cleanup (expect other_programdata_shape FAIL)" = @(
    ,@("  RemoveOtherShapeCopy(Failed);", "  // RemoveOtherShapeCopy(Failed);"))
  "D: uninstall keeps files (expect uninstall FAIL)" = @(
    @("Type: filesandordirs; Name: `"{code:GetPluginRootDir}`"$N", ""),
    @("DestDir: `"{code:GetPluginModuleDir}`"; Flags: ignoreversion;", "DestDir: `"{code:GetPluginModuleDir}`"; Flags: ignoreversion uninsneveruninstall;"))
  "E: legacy data cleanup too wide (expect other_plugins + uninstall FAIL)" = @(
    ,@("Result := ObsDir + '\data\obs-plugins\aitum-dock-sync';", "Result := ObsDir + '\data\obs-plugins';"))
}

$HashBefore = (Get-FileHash $Target).Hash
foreach ($Name in $Breaks.Keys) {
  Copy-Item -Force $Target $Backup
  try {
    $Text = [IO.File]::ReadAllText($Target)
    foreach ($Pair in $Breaks[$Name]) {
      $Good, $Bad = $Pair
      if (-not $Text.Contains($Good)) { throw "Break point not found in ${Name}: $Good" }
      $Text = $Text.Replace($Good, $Bad)
    }
    [IO.File]::WriteAllText($Target, $Text)
    Write-Host "=== BROKEN $Name ==="
    Push-Location $Root
    py -3 -m pytest tests/test_chunk3_cleanup.py
    Pop-Location
  } finally {
    Copy-Item -Force $Backup $Target
    Remove-Item -Force $Backup
    Write-Host "=== RESTORED after $Name : hash match = $($HashBefore -eq (Get-FileHash $Target).Hash) ==="
  }
}
