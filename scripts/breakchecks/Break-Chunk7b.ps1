#Requires -Version 5.1
# test_dock_shows_not_found_without_aitum (added test). The behavior was built in Chunk 7
# before this test existed, so the fail-first run is done against the code with the
# not-found feature stripped out (PRE), then the real code (GREEN), then break checks A-C.
# Every edit is restored and hash-checked.
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Dock = Join-Path $Root "plugin\src\platform-dock.cpp"
$Ctrl = Join-Path $Root "plugin\src\dock-control.cpp"
$Run = Join-Path $Root "scripts\Run-CppTests.ps1"
$Filter = "test_dock_shows_not_found_without_aitum"

$Steps = [ordered]@{
  "PRE (fail-first): not-found feature stripped (expect FAIL)" = @(
    @($Dock, "	const bool found = aitum_multistream_found();", "	const bool found = true;"),
    @($Dock, "	not_found_->setHidden(found);", "	not_found_->setHidden(true);"))
  "GREEN: real code (expect PASS)" = @()
  "A: Aitum dock not required (expect FAIL)" = @(
    ,@($Ctrl, "return QFile::exists(aitum_config_path()) && find_aitum_dock() != nullptr;", "return QFile::exists(aitum_config_path());"))
  "B: message never hidden (expect FAIL)" = @(
    ,@($Dock, "	not_found_->setHidden(found);", "	not_found_->setHidden(false);"))
  "C: outputs listed while not found (expect FAIL)" = @(
    ,@($Dock, "const QStringList outputs = found ? load_aitum_output_names() : QStringList();", "const QStringList outputs = load_aitum_output_names();"))
}

foreach ($Name in $Steps.Keys) {
  $Edits = $Steps[$Name]
  $Files = @($Edits | ForEach-Object { $_[0] } | Select-Object -Unique)
  $Hashes = @{}
  foreach ($F in $Files) { $Hashes[$F] = (Get-FileHash $F).Hash; Copy-Item -Force $F "$F.breakcheck-bak" }
  try {
    foreach ($E in $Edits) {
      $F, $Good, $Bad = $E
      $Text = [IO.File]::ReadAllText($F)
      if (-not $Text.Contains($Good)) { throw "Edit point not found in ${Name}: $Good" }
      [IO.File]::WriteAllText($F, $Text.Replace($Good, $Bad))
    }
    Write-Host "=== $Name ==="
    powershell -NoProfile -ExecutionPolicy Bypass -File $Run -Filter $Filter
  } finally {
    foreach ($F in $Files) {
      Copy-Item -Force "$F.breakcheck-bak" $F
      Remove-Item -Force "$F.breakcheck-bak"
      Write-Host "=== RESTORED $([IO.Path]::GetFileName($F)) after $Name : hash match = $($Hashes[$F] -eq (Get-FileHash $F).Hash) ==="
    }
  }
}
