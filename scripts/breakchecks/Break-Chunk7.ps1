#Requires -Version 5.1
# Break checks for Chunk 7 (PLAN.md rule 8). Each break edits one plugin source file,
# rebuilds and runs the C++ tests (the named ones must FAIL), then restores the file.
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Dock = Join-Path $Root "plugin\src\platform-dock.cpp"
$Main = Join-Path $Root "plugin\src\plugin-main.cpp"

$Breaks = [ordered]@{
  "A: no live action on toggle (expect tick_while_live + untick_while_live FAIL)" = @($Dock,
    "	if (is_live_ && is_live_())", "	if (false)")
  "B: toggle acts even when offline (expect toggle_while_offline FAIL)" = @($Dock,
    "	if (is_live_ && is_live_())", "	if (true)")
  "C: no refresh on profile change (expect profile_change FAIL)" = @($Main,
    "OBS_FRONTEND_EVENT_PROFILE_CHANGED) {`n		run_on_ui_thread([] { refresh_dock(); });",
    "OBS_FRONTEND_EVENT_PROFILE_CHANGED) {`n		// refresh removed")
  "D: saved picks not shown (expect dock_lists + profile_change FAIL)" = @($Dock,
    "box->setChecked(picks_ && picks_->is_picked(profile, box->text()));", "box->setChecked(false);")
  "E: toggle not saved (expect tick/untick/offline FAIL)" = @($Dock,
    "	picks_->set_picked(profile, output, checked);", "	// picks_->set_picked(profile, output, checked);")
}

foreach ($Name in $Breaks.Keys) {
  $Target, $Good, $Bad = $Breaks[$Name]
  $Backup = "$Target.breakcheck-bak"
  $HashBefore = (Get-FileHash $Target).Hash
  Copy-Item -Force $Target $Backup
  try {
    $Text = [IO.File]::ReadAllText($Target)
    if (-not $Text.Contains($Good)) { throw "Break point not found in ${Name}: $Good" }
    [IO.File]::WriteAllText($Target, $Text.Replace($Good, $Bad))
    Write-Host "=== BROKEN $Name ==="
    powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root "scripts\Run-CppTests.ps1")
  } finally {
    Copy-Item -Force $Backup $Target
    Remove-Item -Force $Backup
    Write-Host "=== RESTORED after $Name : hash match = $($HashBefore -eq (Get-FileHash $Target).Hash) ==="
  }
}
