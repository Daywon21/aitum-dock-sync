"""Chunk 3 (PLAN.md): legacy cleanup on install and uninstall.

Every install removes the legacy copies:
  <obs>\\obs-plugins\\64bit\\aitum-dock-sync.dll
  <obs>\\data\\obs-plugins\\aitum-dock-sync\\
  %APPDATA%\\obs-studio\\plugins\\64bit\\aitum-dock-sync.dll
and the other-shape copy inside ProgramData (added test, Captain Dark 2026-11-02).
Uninstall removes the ProgramData plugin folder.
"""
import shutil
import subprocess

import pytest

from conftest import ISS, PACKAGE, FakeObsTree, run_setup, tree_files

PKG_DLL = PACKAGE / "aitum-dock-sync" / "aitum-dock-sync.dll"
PKG_INI = PACKAGE / "aitum-dock-sync" / "data" / "locale" / "en-US.ini"
LEGACY_DIR = ("obs-plugins", "64bit")

FLAT = ["obs-studio/plugins/aitum-dock-sync/aitum-dock-sync.dll",
        "obs-studio/plugins/aitum-dock-sync/data/locale/en-US.ini"]
BIN64 = ["obs-studio/plugins/aitum-dock-sync/bin/64bit/aitum-dock-sync.dll",
         "obs-studio/plugins/aitum-dock-sync/data/locale/en-US.ini"]


def _guard():
    # Setup/uninstall must not reach the real %APPDATA% or the real OBS (registry lookup).
    text = ISS.read_text(encoding="utf-8")
    problems = []
    if "{param:APPDATA" not in text:
        problems.append("no /APPDATA= param, setup would clean the real %APPDATA%")
    if "RemoveInstalledFiles" in text:
        problems.append("uninstall still runs RemoveInstalledFiles, which finds the real OBS via the registry")
    assert problems == [], "Refusing to run setup/uninstall: " + "; ".join(problems)


@pytest.fixture
def c3(test_installer, fake_obs64, sandbox):
    _guard()
    return test_installer, fake_obs64, sandbox


def _write(path, data: bytes):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def _add_legacy_obs_copy(obs):
    _write(obs.dir.joinpath(*LEGACY_DIR, "aitum-dock-sync.dll"), b"FAKE legacy program files dll")
    _write(obs.dir / "data" / "obs-plugins" / "aitum-dock-sync" / "locale" / "en-US.ini",
           b'AitumDockSync="legacy"\n')


def _add_legacy_appdata_copy(sb):
    _write(sb.appdata / "obs-studio" / "plugins" / "64bit" / "aitum-dock-sync.dll", b"FAKE legacy appdata dll")


def _install(exe, sb, obs):
    code, log = run_setup(exe, sb, obs)
    assert code == 0, log


def test_installer_removes_legacy_program_files_copy(c3):
    exe, fake_obs64, sb = c3
    obs = FakeObsTree(sb.root, fake_obs64.get("33.0.0"), portable=False)
    _add_legacy_obs_copy(obs)
    _install(exe, sb, obs)
    assert tree_files(obs.dir) == ["bin/64bit/obs64.exe"]
    assert not (obs.dir / "data" / "obs-plugins" / "aitum-dock-sync").exists()
    assert obs.dir.joinpath(*LEGACY_DIR).is_dir()
    assert tree_files(sb.programdata) == FLAT


def test_installer_removes_appdata_copy(c3):
    exe, fake_obs64, sb = c3
    obs = FakeObsTree(sb.root, fake_obs64.get("33.0.0"), portable=False)
    _add_legacy_appdata_copy(sb)
    _install(exe, sb, obs)
    assert tree_files(sb.appdata) == []
    assert (sb.appdata / "obs-studio" / "plugins" / "64bit").is_dir()
    assert tree_files(sb.programdata) == FLAT


OTHER_OBS = {
    "obs-plugins/64bit/vertical-canvas.dll": bytes(range(256)) * 4,
    "obs-plugins/64bit/captain-multi-chat.dll": bytes(range(255, -1, -1)) * 4,
    "data/obs-plugins/vertical-canvas/locale/en-US.ini": b'VerticalCanvas="Vertical Canvas"\n',
    "data/obs-plugins/captain-multi-chat/locale/en-US.ini": b'CaptainMultiChat="Captain Multi Chat"\n',
}
OTHER_PD = {
    "obs-studio/plugins/StreamDeckPlugin/bin/64bit/StreamDeckPlugin.dll": b"FAKE StreamDeckPlugin dll" * 16,
    "obs-studio/plugins/StreamDeckPlugin/data/locale/en-US.ini": b'StreamDeck="Stream Deck"\n',
}
OTHER_AD = {
    "obs-studio/plugins/64bit/other-user-plugin.dll": b"FAKE other user plugin" * 8,
}


def _add_others(obs, sb):
    for rel, data in OTHER_OBS.items():
        _write(obs.dir / rel, data)
    for rel, data in OTHER_PD.items():
        _write(sb.programdata / rel, data)
    for rel, data in OTHER_AD.items():
        _write(sb.appdata / rel, data)


def _assert_others_identical(obs, sb):
    for base, files in ((obs.dir, OTHER_OBS), (sb.programdata, OTHER_PD), (sb.appdata, OTHER_AD)):
        for rel, data in files.items():
            assert (base / rel).read_bytes() == data, rel


def test_installer_leaves_other_plugins_untouched(c3):
    exe, fake_obs64, sb = c3
    obs = FakeObsTree(sb.root, fake_obs64.get("33.0.0"), portable=False)
    _add_others(obs, sb)
    _add_legacy_obs_copy(obs)
    _add_legacy_appdata_copy(sb)
    _install(exe, sb, obs)
    _assert_others_identical(obs, sb)
    assert tree_files(obs.dir) == sorted(["bin/64bit/obs64.exe"] + list(OTHER_OBS))
    assert tree_files(sb.programdata) == sorted(FLAT + list(OTHER_PD))
    assert tree_files(sb.appdata) == sorted(OTHER_AD)


def test_uninstall_removes_programdata_folder(c3):
    exe, fake_obs64, sb = c3
    obs = FakeObsTree(sb.root, fake_obs64.get("33.0.0"), portable=False)
    _add_others(obs, sb)
    _install(exe, sb, obs)
    assert tree_files(sb.programdata) == sorted(FLAT + list(OTHER_PD))
    uninstaller = sb.app_dir / "unins000.exe"
    assert uninstaller.is_file()
    log = sb.root / "uninstall.log"
    run = subprocess.run([str(uninstaller), "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", f"/LOG={log}"],
                         capture_output=True, text=True, timeout=180)
    assert run.returncode == 0, log.read_text(errors="replace") if log.exists() else "(no uninstall log)"
    assert not (sb.programdata / "obs-studio" / "plugins" / "aitum-dock-sync").exists()
    assert tree_files(sb.programdata) == sorted(OTHER_PD)
    _assert_others_identical(obs, sb)
    assert tree_files(obs.dir) == sorted(["bin/64bit/obs64.exe"] + list(OTHER_OBS))


def test_installer_removes_other_programdata_shape(c3):
    exe, fake_obs64, sb = c3
    obs = FakeObsTree(sb.root, fake_obs64.get("32.2.2"), portable=False)
    obs64 = obs.dir / "bin" / "64bit" / "obs64.exe"
    plugin = sb.programdata / "obs-studio" / "plugins" / "aitum-dock-sync"

    _install(exe, sb, obs)
    assert tree_files(sb.programdata) == BIN64

    shutil.copy2(fake_obs64.get("33.0.0"), obs64)
    _install(exe, sb, obs)
    assert tree_files(sb.programdata) == FLAT
    assert not (plugin / "bin").exists()
    assert (plugin / "aitum-dock-sync.dll").read_bytes() == PKG_DLL.read_bytes()

    shutil.copy2(fake_obs64.get("32.2.2"), obs64)
    _install(exe, sb, obs)
    assert tree_files(sb.programdata) == BIN64
    assert (plugin / "bin" / "64bit" / "aitum-dock-sync.dll").read_bytes() == PKG_DLL.read_bytes()
    assert (plugin / "data" / "locale" / "en-US.ini").read_bytes() == PKG_INI.read_bytes()
