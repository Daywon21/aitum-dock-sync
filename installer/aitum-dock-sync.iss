; Aitum Dock Sync Windows installer

#ifndef MyAppVersion
#define MyAppVersion "2.0.0"
#endif

#ifndef MyAppName
#define MyAppName "Aitum Dock Sync"
#endif
#ifndef MyAppId
#define MyAppId "{{8F3A1C2E-9B47-4D60-A8E1-AITUMDOCKSYNC}"
#endif
#define MyAppPublisher "Captain Dark"
#ifndef PackageRoot
#define PackageRoot "..\release\Package"
#endif

[Setup]
AppId={#MyAppId}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\Aitum Dock Sync
DefaultGroupName={#MyAppName}
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
OutputDir=output
OutputBaseFilename=AitumDockSync-Setup-{#MyAppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
DirExistsWarning=no
CloseApplications=force
UninstallDisplayName={#MyAppName}
InfoBeforeFile=INSTALL-NOTES.txt

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
english.ObsNotFound=OBS Studio (64-bit) was not found. Install OBS from obsproject.com first.
english.ObsDirInvalid=The OBS folder given with /OBSDIR= does not exist:%n%n%1
english.ObsVersionUnknown=Could not read the OBS version from:%n%n%1
english.ObsCloseTitle=Before you continue
english.ObsCloseSubtitle=Close OBS Studio completely before installing.
english.ObsCloseBody=Aitum Dock Sync installs into your OBS Studio folder.%n%nClose OBS completely, including the system tray icon, before you click Next.%n%nOBS found at:%n%1
english.AfterInstall=Installation complete.%n%n1. Restart OBS Studio.%n2. Open Tools, Scripts and remove aitum_sync.py if it is still listed.%n3. Start streaming. Aitum Multistream outputs start and stop with your main stream.
english.UninstallObsRunning=OBS Studio is still running.%n%nClose OBS completely, including the system tray icon, then click Retry.%n%nClick Cancel to abort uninstall.
english.LegacyCleanupIncomplete=Some old Aitum Dock Sync files could not be removed. Close OBS, then delete them manually:%n%n%1

[Files]
Source: "{#PackageRoot}\aitum-dock-sync\aitum-dock-sync.dll"; DestDir: "{code:GetPluginModuleDir}"; Flags: ignoreversion; Check: ObsDirValid
Source: "{#PackageRoot}\aitum-dock-sync\data\*"; DestDir: "{code:GetPluginDataDir}"; Flags: ignoreversion recursesubdirs createallsubdirs; Check: ObsDirValid

[Icons]
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[UninstallDelete]
Type: filesandordirs; Name: "{app}"
; Paths are fixed at install time (no registry lookup at uninstall).
Type: filesandordirs; Name: "{code:GetPluginRootDir}"
Type: files; Name: "{code:GetLegacyModuleFile}"
Type: filesandordirs; Name: "{code:GetLegacyDataDir}"
Type: files; Name: "{code:GetLegacyAppDataFile}"

[Code]
var
  ObsDir: String;
  ObsMajor: Integer;
  ObsPortable: Boolean;
  PluginRoot: String;
  ObsClosePage: TWizardPage;
  ObsCloseLabel: TNewStaticText;

function ObsDirValid: Boolean;
begin
  Result := (ObsDir <> '') and DirExists(ObsDir);
end;

// OBS treats these files in its root folder as portable mode (OBS 33.0.1 frontend/obs-main.cpp).
function IsPortableObs(const Dir: String): Boolean;
begin
  Result := FileExists(Dir + '\portable_mode') or FileExists(Dir + '\obs_portable_mode') or
            FileExists(Dir + '\portable_mode.txt') or FileExists(Dir + '\obs_portable_mode.txt');
end;

function GetObsMajorVersion(const Dir: String): Integer;
var
  MS, LS: Cardinal;
begin
  Result := -1;
  if GetVersionNumbers(Dir + '\bin\64bit\obs64.exe', MS, LS) then
    Result := MS shr 16;
end;

function GetProgramDataRoot: String;
begin
  Result := ExpandConstant('{param:PROGRAMDATA|}');
  if Result = '' then
    Result := ExpandConstant('{commonappdata}');
  Result := RemoveBackslashUnlessRoot(Result);
end;

// OBS 33+ (and portable): <root>\aitum-dock-sync.dll. OBS < 33: <root>\bin\64bit\aitum-dock-sync.dll.
function GetPluginModuleDir(Param: String): String;
begin
  if (not ObsPortable) and (ObsMajor < 33) then
    Result := PluginRoot + '\bin\64bit'
  else
    Result := PluginRoot;
end;

function GetPluginDataDir(Param: String): String;
begin
  Result := PluginRoot + '\data';
end;

function FindObsInstall: String;
var
  RegPath: String;
begin
  Result := '';
  if RegQueryStringValue(HKLM64, 'SOFTWARE\OBS Studio', '', RegPath) then
  begin
    if DirExists(RegPath) then
    begin
      Result := RegPath;
      Exit;
    end;
  end;
  if DirExists(ExpandConstant('{pf}\obs-studio')) then
    Result := ExpandConstant('{pf}\obs-studio')
  else if DirExists(ExpandConstant('{pf32}\obs-studio')) then
    Result := ExpandConstant('{pf32}\obs-studio');
end;

function IsProcessListed(const ImageName: String): Boolean;
var
  ResultCode: Integer;
begin
  Result := False;
  if Exec(
       ExpandConstant('{cmd}'),
       '/C tasklist /FI "IMAGENAME eq ' + ImageName + '" 2>nul | find /I "' + ImageName + '" >nul',
       '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
    Result := (ResultCode = 0);
end;

function IsObsRunning: Boolean;
begin
  Result := IsProcessListed('obs64.exe') or IsProcessListed('obs32.exe') or IsProcessListed('obs.exe');
end;

function WaitForObsClosed: Boolean;
var
  Answer: Integer;
begin
  Result := True;
  while IsObsRunning do
  begin
    Answer := SuppressibleMsgBox(ExpandConstant('{cm:UninstallObsRunning}'), mbConfirmation, MB_RETRYCANCEL, IDCANCEL);
    if Answer <> IDRETRY then
    begin
      Result := False;
      Exit;
    end;
  end;
end;

function RemovePluginFile(const Path: String): Boolean;
begin
  Result := True;
  if FileExists(Path) then
    Result := DeleteFile(Path);
end;

function RemovePluginTree(const Path: String): Boolean;
begin
  Result := True;
  if DirExists(Path) then
    Result := DelTree(Path, True, True, True);
end;

function GetAppDataRoot: String;
begin
  Result := ExpandConstant('{param:APPDATA|}');
  if Result = '' then
    Result := ExpandConstant('{userappdata}');
  Result := RemoveBackslashUnlessRoot(Result);
end;

function GetPluginRootDir(Param: String): String;
begin
  Result := PluginRoot;
end;

// ; BEGIN legacy-cleanup
// Legacy copies of this plugin. Removed on every install and on uninstall.
function GetLegacyModuleFile(Param: String): String;
begin
  Result := ObsDir + '\obs-plugins\64bit\aitum-dock-sync.dll';
end;

function GetLegacyDataDir(Param: String): String;
begin
  Result := ObsDir + '\data\obs-plugins\aitum-dock-sync';
end;

function GetLegacyAppDataFile(Param: String): String;
begin
  Result := GetAppDataRoot + '\obs-studio\plugins\64bit\aitum-dock-sync.dll';
end;
// ; END legacy-cleanup

// The ProgramData copy in the shape this install does not use, so only one copy exists.
function RemoveOtherShapeCopy(var Failed: String): Boolean;
var
  Path: String;
begin
  Result := True;
  if GetPluginModuleDir('') = PluginRoot then
  begin
    Path := PluginRoot + '\bin';
    if not RemovePluginTree(Path) then
      Result := False;
  end
  else
  begin
    Path := PluginRoot + '\aitum-dock-sync.dll';
    if not RemovePluginFile(Path) then
      Result := False;
  end;
  if not Result then
    Failed := Failed + Path + #13#10;
end;

procedure CleanupBeforeInstall;
var
  Failed: String;
begin
  Failed := '';
  if not RemovePluginFile(GetLegacyModuleFile('')) then
    Failed := Failed + GetLegacyModuleFile('') + #13#10;
  if not RemovePluginTree(GetLegacyDataDir('')) then
    Failed := Failed + GetLegacyDataDir('') + #13#10;
  if not RemovePluginFile(GetLegacyAppDataFile('')) then
    Failed := Failed + GetLegacyAppDataFile('') + #13#10;
  RemoveOtherShapeCopy(Failed);

  if Failed <> '' then
  begin
    Log('Cleanup failed for: ' + Failed);
    SuppressibleMsgBox(FmtMessage(ExpandConstant('{cm:LegacyCleanupIncomplete}'), [Failed]), mbError, MB_OK, IDOK);
  end;
end;

function InitializeSetup: Boolean;
var
  ObsParam: String;
begin
  Result := False;
  ObsParam := ExpandConstant('{param:OBSDIR|}');
  if ObsParam <> '' then
  begin
    ObsDir := RemoveBackslashUnlessRoot(ObsParam);
    if not DirExists(ObsDir) then
    begin
      SuppressibleMsgBox(FmtMessage(ExpandConstant('{cm:ObsDirInvalid}'), [ObsDir]), mbError, MB_OK, IDOK);
      Exit;
    end;
  end
  else
    ObsDir := FindObsInstall;

  if ObsDir = '' then
  begin
    SuppressibleMsgBox(ExpandConstant('{cm:ObsNotFound}'), mbError, MB_OK, IDOK);
    Exit;
  end;

  ObsMajor := GetObsMajorVersion(ObsDir);
  if ObsMajor < 0 then
  begin
    SuppressibleMsgBox(FmtMessage(ExpandConstant('{cm:ObsVersionUnknown}'), [ObsDir + '\bin\64bit\obs64.exe']), mbError, MB_OK, IDOK);
    Exit;
  end;

  ObsPortable := IsPortableObs(ObsDir);
  if ObsPortable then
    PluginRoot := ObsDir + '\plugins\aitum-dock-sync'
  else
    PluginRoot := GetProgramDataRoot + '\obs-studio\plugins\aitum-dock-sync';

  Log(Format('OBS folder: %s, major version: %d, portable: %d', [ObsDir, ObsMajor, Ord(ObsPortable)]));
  Log('Plugin module folder: ' + GetPluginModuleDir(''));
  Log('Plugin data folder: ' + GetPluginDataDir(''));
  Result := True;
end;

procedure InitializeWizard;
begin
  ObsClosePage := CreateCustomPage(
    wpWelcome,
    ExpandConstant('{cm:ObsCloseTitle}'),
    ExpandConstant('{cm:ObsCloseSubtitle}'));
  ObsCloseLabel := TNewStaticText.Create(ObsClosePage);
  ObsCloseLabel.Parent := ObsClosePage.Surface;
  ObsCloseLabel.AutoSize := True;
  ObsCloseLabel.WordWrap := True;
  ObsCloseLabel.Width := ObsClosePage.SurfaceWidth;
  ObsCloseLabel.Caption := FmtMessage(ExpandConstant('{cm:ObsCloseBody}'), [ObsDir]);
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssInstall then
    CleanupBeforeInstall;
  if (CurStep = ssPostInstall) and (not WizardSilent) then
    MsgBox(ExpandConstant('{cm:AfterInstall}'), mbInformation, MB_OK);
end;

function InitializeUninstall: Boolean;
begin
  Result := WaitForObsClosed;
end;
