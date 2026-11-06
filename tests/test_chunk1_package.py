r"""Chunk 1 (PLAN.md): package layout.

(test_no_hardcoded_obs_plugins_64bit moved to Chunk 2, Captain Dark 2026-11-02, option A.)

Build-Installer.ps1 must stage
    release\Package\aitum-dock-sync\aitum-dock-sync.dll
    release\Package\aitum-dock-sync\data\...
and nothing under obs-plugins\64bit.
"""
import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
PACKAGE = ROOT / "release" / "Package"


@pytest.fixture(scope="module")
def staged_package():
    # Start from an empty Package folder so only what Build-Installer.ps1 stages now is inspected.
    if PACKAGE.exists():
        shutil.rmtree(PACKAGE)
    run = subprocess.run(
        ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass",
         "-File", str(ROOT / "scripts" / "Build-Installer.ps1")],
        cwd=ROOT, capture_output=True, text=True,
    )
    assert run.returncode == 0, run.stdout + run.stderr
    return PACKAGE


def test_package_uses_new_plugin_layout(staged_package):
    files = sorted(
        p.relative_to(staged_package).as_posix()
        for p in staged_package.rglob("*") if p.is_file()
    )
    assert files == [
        "aitum-dock-sync/aitum-dock-sync.dll",
        "aitum-dock-sync/data/locale/en-US.ini",
    ]
    assert (staged_package / "aitum-dock-sync" / "aitum-dock-sync.dll").read_bytes() == \
        (ROOT / "plugin" / "build" / "aitum-dock-sync.dll").read_bytes()
    assert (staged_package / "aitum-dock-sync" / "data" / "locale" / "en-US.ini").read_bytes() == \
        (ROOT / "plugin" / "data" / "locale" / "en-US.ini").read_bytes()
