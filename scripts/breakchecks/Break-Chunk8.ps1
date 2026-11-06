#Requires -Version 5.1
# Chunk 8 break checks (PLAN.md rule 8). Every edit is restored and hash-checked.
#  PRE/GREEN: test_no_timer_polling is a guard that today's code already passed, so its
#             fail-first is the refresh queued with a QTimer instead (PRE), then real code.
#  A-D: C++ refresh tests.
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Dock = Join-Path $Root "plugin\src\platform-dock.cpp"
$Run = Join-Path $Root "scripts\Run-CppTests.ps1"
$Filter = "test_config_change|test_new_aitum_groupbox|test_removed_output"
$N = "`n"

function Invoke-Pytest { Push-Location $Root; py -3 -m pytest tests/test_chunk8_no_polling.py; Pop-Location }
function Invoke-Ctest { powershell -NoProfile -ExecutionPolicy Bypass -File $Run -Filter $Filter }

$Steps = [ordered]@{
  "PRE (fail-first no_timer_polling): refresh queued with QTimer (expect FAIL)" = @("py", @(
    @($Dock, "#include <QMetaObject>$N", "#include <QMetaObject>$N#include <QTimer>$N"),
    @($Dock, "	QMetaObject::invokeMethod(${N}		this,${N}		[this] {", "	QTimer::singleShot(${N}		0, this,${N}		[this] {"),
    @($Dock, "		},${N}		Qt::QueuedConnection);", "		});")))
  "GREEN no_timer_polling: real code (expect PASS)" = @("py", @())
  "A: watcher only on the file, never re-armed (expect config_change FAIL)" = @("ct", @(
    @($Dock, "	if (QFileInfo::exists(dir) && !watcher_->directories().contains(dir))${N}		watcher_->addPath(dir);${N}", ""),
    @($Dock, "void PlatformDock::on_config_event()${N}{${N}	arm_watcher();", "void PlatformDock::on_config_event()${N}{")))
  "B: no file watcher (expect config_change + removed_output FAIL)" = @("ct", @(
    ,@($Dock, "	arm_watcher();${N}}${N}${N}// Editors", "}${N}${N}// Editors")))
  "C: no event filter refresh (expect new_aitum_groupbox FAIL)" = @("ct", @(
    ,@($Dock, "			if (qobject_cast<QGroupBox *>(child) || qobject_cast<QGroupBox *>(watched))${N}				queue_refresh();", "			// filter refresh removed")))
  "D: list not rebuilt when outputs change (expect all three FAIL)" = @("ct", @(
    ,@($Dock, "profile != shown_profile_ || outputs != shown_outputs_ ||", "profile != shown_profile_ ||")))
}

foreach ($Name in $Steps.Keys) {
  $Kind, $Edits = $Steps[$Name]
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
    if ($Kind -eq "py") { Invoke-Pytest } else { Invoke-Ctest }
  } finally {
    foreach ($F in $Files) {
      Copy-Item -Force "$F.breakcheck-bak" $F
      Remove-Item -Force "$F.breakcheck-bak"
      Write-Host "=== RESTORED $([IO.Path]::GetFileName($F)) after $Name : hash match = $($Hashes[$F] -eq (Get-FileHash $F).Hash) ==="
    }
  }
}
