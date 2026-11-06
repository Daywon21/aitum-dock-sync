"""Shared fixtures for the installer chunks (2-3).

Fakes live only at the OS/OBS boundary and are named Fake*:
  FakeObs64Builder  - stub obs64.exe carrying a chosen file version (built with MSVC rc + cl)
  FakeObsTree       - fake OBS install folder (installed or portable)
  FakeProgramData   - stand-in for C:\\ProgramData, passed to setup as /PROGRAMDATA=

The installer under test is the real aitum-dock-sync.iss compiled by ISCC, with only
the app identity changed (/DMyAppId, /DMyAppName) so it never touches Captain Dark's
real "Aitum Dock Sync" Apps entry or Start Menu folder (Captain Dark 2026-11-02).
"""
import ctypes
import os
import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
ISS = ROOT / "installer" / "aitum-dock-sync.iss"
PACKAGE = ROOT / "release" / "Package"
VCVARS = r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

TEST_APP_ID = "{{8F3A1C2E-9B47-4D60-A8E1-AITUMDOCKSYNC-TEST}"
TEST_APP_NAME = "Aitum Dock Sync TEST"
TEST_UNINSTALL_KEY = (r"HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall"
                      r"\{8F3A1C2E-9B47-4D60-A8E1-AITUMDOCKSYNC-TEST}_is1")
TEST_START_MENU = Path(os.environ.get("ProgramData", r"C:\ProgramData")) / \
    "Microsoft" / "Windows" / "Start Menu" / "Programs" / TEST_APP_NAME

# Setup must understand these before a test may run it; otherwise it would find the
# real OBS through the registry and install into it.
SANDBOX_PARAMS = ("{param:OBSDIR", "{param:PROGRAMDATA")


def find_iscc():
    for c in (os.path.expandvars(r"%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"),
              os.path.expandvars(r"%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"),
              os.path.expandvars(r"%ProgramFiles%\Inno Setup 6\ISCC.exe")):
        if Path(c).is_file():
            return c
    raise AssertionError("ISCC.exe (Inno Setup 6) not found")


def tree_files(root: Path):
    if not root.exists():
        return []
    return sorted(p.relative_to(root).as_posix() for p in root.rglob("*") if p.is_file())


@pytest.fixture(scope="session")
def require_admin():
    assert ctypes.windll.shell32.IsUserAnAdmin() == 1, \
        "Run pytest from an elevated (Administrator) PowerShell: setup needs admin."


@pytest.fixture(scope="session")
def built_package():
    if PACKAGE.exists():
        shutil.rmtree(PACKAGE)
    run = subprocess.run(
        ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass",
         "-File", str(ROOT / "scripts" / "Build-Installer.ps1")],
        cwd=ROOT, capture_output=True, text=True,
    )
    assert run.returncode == 0, run.stdout + run.stderr
    return PACKAGE


@pytest.fixture(scope="session")
def test_installer(require_admin, built_package, tmp_path_factory):
    text = ISS.read_text(encoding="utf-8")
    missing = [p for p in SANDBOX_PARAMS if p not in text]
    assert missing == [], ("Refusing to run setup: aitum-dock-sync.iss has no sandbox "
                           f"params {missing}, so it would install into the real OBS.")
    out = tmp_path_factory.mktemp("FakeInstallerBuild")
    run = subprocess.run(
        [find_iscc(), "/Q", "/DMyAppVersion=1.0.0", f"/DPackageRoot={built_package}",
         f"/DMyAppId={TEST_APP_ID}", f"/DMyAppName={TEST_APP_NAME}",
         f"/O{out}", "/FAitumDockSync-Setup-TEST", str(ISS)],
        capture_output=True, text=True,
    )
    assert run.returncode == 0, run.stdout + run.stderr
    exe = out / "AitumDockSync-Setup-TEST.exe"
    assert exe.is_file()
    return exe


class FakeObs64Builder:
    """Builds a do-nothing obs64.exe whose VERSIONINFO is the given x.y.z."""

    def __init__(self, cache: Path):
        self.cache = cache
        self.built = {}

    def get(self, version: str) -> Path:
        if version in self.built:
            return self.built[version]
        major, minor, patch = version.split(".")
        work = self.cache / f"FakeObs64-{version}"
        work.mkdir(parents=True)
        (work / "fake_obs64.c").write_text("int main(void) { return 0; }\n")
        (work / "fake_obs64.rc").write_text(f"""#include <winver.h>
1 VERSIONINFO
FILEVERSION {major},{minor},{patch},0
PRODUCTVERSION {major},{minor},{patch},0
FILEOS VOS_NT_WINDOWS32
FILETYPE VFT_APP
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "040904b0"
    BEGIN
      VALUE "FileVersion", "{version}"
      VALUE "ProductVersion", "{version}"
      VALUE "ProductName", "FakeObs64"
    END
  END
  BLOCK "VarFileInfo"
  BEGIN
    VALUE "Translation", 0x409, 1200
  END
END
""")
        cmd = (f'call "{VCVARS}" >nul && cd /d "{work}" && rc /nologo fake_obs64.rc'
               f' && cl /nologo fake_obs64.c fake_obs64.res /Fe:obs64.exe')
        run = subprocess.run(f'cmd /s /c "{cmd}"', capture_output=True, text=True)
        exe = work / "obs64.exe"
        assert run.returncode == 0 and exe.is_file(), run.stdout + run.stderr
        check = subprocess.run(
            ["powershell", "-NoProfile", "-Command", f"(Get-Item '{exe}').VersionInfo.FileVersion"],
            capture_output=True, text=True)
        assert check.stdout.strip() == version, check.stdout + check.stderr
        self.built[version] = exe
        return exe


@pytest.fixture(scope="session")
def fake_obs64(tmp_path_factory):
    return FakeObs64Builder(tmp_path_factory.mktemp("FakeObs64Cache"))


class FakeObsTree:
    def __init__(self, root: Path, obs64: Path, portable: bool):
        self.dir = root / "FakeObs"
        (self.dir / "bin" / "64bit").mkdir(parents=True)
        shutil.copy2(obs64, self.dir / "bin" / "64bit" / "obs64.exe")
        (self.dir / "obs-plugins" / "64bit").mkdir(parents=True)
        (self.dir / "data" / "obs-plugins").mkdir(parents=True)
        if portable:
            (self.dir / "portable_mode.txt").write_text("")


class FakeSandbox:
    def __init__(self, root: Path):
        self.root = root
        self.programdata = root / "FakeProgramData"
        self.programdata.mkdir()
        self.appdata = root / "FakeAppData"
        self.appdata.mkdir()
        self.app_dir = root / "FakeApp"
        self.log = root / "setup.log"


def _remove_test_registration():
    subprocess.run(["reg", "delete", TEST_UNINSTALL_KEY, "/f", "/reg:64"],
                   capture_output=True, text=True)
    if TEST_START_MENU.exists():
        shutil.rmtree(TEST_START_MENU)


@pytest.fixture
def sandbox(tmp_path):
    _remove_test_registration()
    yield FakeSandbox(tmp_path)
    _remove_test_registration()


def run_setup(exe: Path, sb: FakeSandbox, obs: FakeObsTree):
    run = subprocess.run(
        [str(exe), "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART",
         f"/DIR={sb.app_dir}", f"/OBSDIR={obs.dir}", f"/PROGRAMDATA={sb.programdata}",
         f"/APPDATA={sb.appdata}",
         f"/LOG={sb.log}"],
        capture_output=True, text=True, timeout=180,
    )
    log = sb.log.read_text(encoding="utf-8", errors="replace") if sb.log.exists() else "(no setup log)"
    return run.returncode, log
