#Requires -Version 5.1
# Break checks for Chunk 6 (PLAN.md rule 8). Each break edits plugin\src\plugin-main.cpp,
# rebuilds and runs the C++ tests (the named ones must FAIL), then restores the file.
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Target = Join-Path $Root "plugin\src\plugin-main.cpp"
$Backup = "$Target.breakcheck-bak"

$Breaks = [ordered]@{
  "A: ticked filter off, all outputs start (expect start_clicks_only_ticked FAIL)" = @(
    ,@("	if (start) {`n		const QString profile", "	if (false) {`n		const QString profile"))
  "B: ticked filter also on stop (expect stop_stops_all FAIL)" = @(
    ,@("	if (start) {`n		const QString profile", "	if (true) {`n		const QString profile"))
  "C: start delay 1500 -> 1000 ms (expect start_clicks_only_ticked FAIL)" = @(
    ,@("constexpr int kStartDelayMs = 1500;", "constexpr int kStartDelayMs = 1000;"))
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
