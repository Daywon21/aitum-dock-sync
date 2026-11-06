#Requires -Version 5.1
# Break checks for Chunk 1 (PLAN.md rule 8). Each break edits Build-Installer.ps1,
# runs the Chunk 1 test (must FAIL), then restores it and confirms byte-identical.
#  A: DLL staged into the legacy bin\64bit shape (installer compile fails -> fixture error)
#  B: correct package plus a leftover copy in Package (old Program Files layout) (installer still
#     compiles -> the test's own file-list assert must fail)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Target = Join-Path $Root "scripts\Build-Installer.ps1"
$Backup = "$Target.breakcheck-bak"

$Breaks = [ordered]@{
  "A: DLL staged to Package\aitum-dock-sync\bin\64bit" = @(
    'Join-Path $PackageRoot "aitum-dock-sync"',
    'Join-Path $PackageRoot "aitum-dock-sync\bin\64bit"')
  "B: extra legacy copy staged to Package (old Program Files layout)" = @(
    'Copy-Item -Recurse -Force (Join-Path $PluginRoot "data") $DataOut',
    ('Copy-Item -Recurse -Force (Join-Path $PluginRoot "data") $DataOut' + "`r`n" +
     '$Legacy = Join-Path $PackageRoot ("obs-plugins" + [IO.Path]::DirectorySeparatorChar + "64" + "bit"); New-Item -ItemType Directory -Force -Path $Legacy | Out-Null; Copy-Item -Force (Join-Path $PluginRoot "build\aitum-dock-sync.dll") $Legacy'))
}

$HashBefore = (Get-FileHash $Target).Hash
foreach ($Name in $Breaks.Keys) {
  $Good, $Bad = $Breaks[$Name]
  Copy-Item -Force $Target $Backup
  try {
    $Text = [IO.File]::ReadAllText($Target)
    if (-not $Text.Contains($Good)) { throw "Break point not found: $Name" }
    [IO.File]::WriteAllText($Target, $Text.Replace($Good, $Bad))
    Write-Host "=== BROKEN $Name ==="
    Push-Location $Root
    py -3 -m pytest tests/test_chunk1_package.py
    Pop-Location
  } finally {
    Copy-Item -Force $Backup $Target
    Remove-Item -Force $Backup
    Write-Host "=== RESTORED after $Name : hash match = $($HashBefore -eq (Get-FileHash $Target).Hash) ==="
  }
}
