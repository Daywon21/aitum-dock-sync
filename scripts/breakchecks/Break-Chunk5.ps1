#Requires -Version 5.1
# Break checks for Chunk 5 (PLAN.md rule 8). Each break edits plugin\src\picks-store.cpp,
# rebuilds and runs the C++ tests (the named ones must FAIL), then restores the file.
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Target = Join-Path $Root "plugin\src\picks-store.cpp"
$Backup = "$Target.breakcheck-bak"

$Breaks = [ordered]@{
  "A: set_picked does not save (expect saved_per_profile + survive_reload FAIL)" = @(
    ,@("	save_file(file_path_, picks_);", "	// save_file(file_path_, picks_);"))
  "B: saved file not loaded (expect survive_reload + unknown_output FAIL)" = @(
    ,@("file_path_(file_path), picks_(load_file(file_path))", "file_path_(file_path), picks_()"))
  "C: unknown output defaults ticked (expect saved_per_profile + survive_reload + unknown_output FAIL)" = @(
    ,@(".value(output).toBool(false);", ".value(output).toBool(true);"))
  "D: config folder not created (expect saved_per_profile FAIL)" = @(
    ,@("	QDir().mkpath(QFileInfo(path).absolutePath());", "	// QDir().mkpath(QFileInfo(path).absolutePath());"))
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
    powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root "scripts\Run-CppTests.ps1")
  } finally {
    Copy-Item -Force $Backup $Target
    Remove-Item -Force $Backup
    Write-Host "=== RESTORED after $Name : hash match = $($HashBefore -eq (Get-FileHash $Target).Hash) ==="
  }
}
