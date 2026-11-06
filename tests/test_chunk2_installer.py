"""Chunk 2 (PLAN.md): installer targets.

test_no_hardcoded_obs_plugins_64bit was written in Chunk 1 and moved here unchanged
(Captain Dark 2026-11-02, option A). Fail-first run: run-chunk1-failfirst.txt.
"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


# Code files only. Docs (README.md, MANUAL_TEST.md, INSTALL-NOTES.txt) may name the legacy path.
# tests\ is skipped because this file has to contain the pattern.
CODE_SUFFIXES = {".iss", ".ps1", ".cpp", ".hpp", ".h", ".py", ".cmake"}
CODE_NAMES = {"CMakeLists.txt"}
SKIP_DIRS = {"_snapshots", "release", "build", "tests", "output", "__pycache__"}
PATTERN = re.compile(r"obs-plugins[\\/]+64bit", re.IGNORECASE)
BEGIN = "; BEGIN legacy-cleanup"
END = "; END legacy-cleanup"


def code_files():
    for p in sorted(ROOT.rglob("*")):
        rel = p.relative_to(ROOT)
        if not p.is_file() or any(part in SKIP_DIRS for part in rel.parts):
            continue
        if p.suffix.lower() in CODE_SUFFIXES or p.name in CODE_NAMES:
            yield p


def test_no_hardcoded_obs_plugins_64bit():
    offenders = []
    for path in code_files():
        inside = False
        for n, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            if BEGIN in line:
                inside = True
                continue
            if END in line:
                inside = False
                continue
            if PATTERN.search(line) and not inside:
                offenders.append(f"{path.relative_to(ROOT).as_posix()}:{n}: {line.strip()}")
    assert offenders == []


# ---- Installer targets (written in Chunk 2) ----
from conftest import PACKAGE, FakeObsTree, run_setup, tree_files  # noqa: E402

PKG_DLL = PACKAGE / "aitum-dock-sync" / "aitum-dock-sync.dll"
PKG_INI = PACKAGE / "aitum-dock-sync" / "data" / "locale" / "en-US.ini"


def _install(test_installer, fake_obs64, sandbox, version, portable=False):
    obs = FakeObsTree(sandbox.root, fake_obs64.get(version), portable)
    obs_before = tree_files(obs.dir)
    code, log = run_setup(test_installer, sandbox, obs)
    assert code == 0, log
    return obs, obs_before


def test_installer_obs32_targets_programdata_bin64bit(test_installer, fake_obs64, sandbox):
    obs, obs_before = _install(test_installer, fake_obs64, sandbox, "32.2.2")
    plugin = sandbox.programdata / "obs-studio" / "plugins" / "aitum-dock-sync"
    assert tree_files(sandbox.programdata) == [
        "obs-studio/plugins/aitum-dock-sync/bin/64bit/aitum-dock-sync.dll",
        "obs-studio/plugins/aitum-dock-sync/data/locale/en-US.ini",
    ]
    assert (plugin / "bin" / "64bit" / "aitum-dock-sync.dll").read_bytes() == PKG_DLL.read_bytes()
    assert (plugin / "data" / "locale" / "en-US.ini").read_bytes() == PKG_INI.read_bytes()
    assert tree_files(obs.dir) == obs_before


def test_installer_obs33_targets_programdata_flat(test_installer, fake_obs64, sandbox):
    obs, obs_before = _install(test_installer, fake_obs64, sandbox, "33.0.0")
    plugin = sandbox.programdata / "obs-studio" / "plugins" / "aitum-dock-sync"
    assert tree_files(sandbox.programdata) == [
        "obs-studio/plugins/aitum-dock-sync/aitum-dock-sync.dll",
        "obs-studio/plugins/aitum-dock-sync/data/locale/en-US.ini",
    ]
    assert (plugin / "aitum-dock-sync.dll").read_bytes() == PKG_DLL.read_bytes()
    assert (plugin / "data" / "locale" / "en-US.ini").read_bytes() == PKG_INI.read_bytes()
    assert tree_files(obs.dir) == obs_before


def test_installer_obs34_targets_programdata_flat(test_installer, fake_obs64, sandbox):
    obs, obs_before = _install(test_installer, fake_obs64, sandbox, "34.0.0")
    plugin = sandbox.programdata / "obs-studio" / "plugins" / "aitum-dock-sync"
    assert tree_files(sandbox.programdata) == [
        "obs-studio/plugins/aitum-dock-sync/aitum-dock-sync.dll",
        "obs-studio/plugins/aitum-dock-sync/data/locale/en-US.ini",
    ]
    assert (plugin / "aitum-dock-sync.dll").read_bytes() == PKG_DLL.read_bytes()
    assert (plugin / "data" / "locale" / "en-US.ini").read_bytes() == PKG_INI.read_bytes()
    assert tree_files(obs.dir) == obs_before


def test_installer_portable_targets_obs_plugins_dir(test_installer, fake_obs64, sandbox):
    obs, obs_before = _install(test_installer, fake_obs64, sandbox, "33.0.0", portable=True)
    plugin = obs.dir / "plugins" / "aitum-dock-sync"
    assert tree_files(obs.dir) == sorted(obs_before + [
        "plugins/aitum-dock-sync/aitum-dock-sync.dll",
        "plugins/aitum-dock-sync/data/locale/en-US.ini",
    ])
    assert (plugin / "aitum-dock-sync.dll").read_bytes() == PKG_DLL.read_bytes()
    assert (plugin / "data" / "locale" / "en-US.ini").read_bytes() == PKG_INI.read_bytes()
    assert tree_files(sandbox.programdata) == []
