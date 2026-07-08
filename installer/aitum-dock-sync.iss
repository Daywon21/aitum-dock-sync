; Aitum Dock Sync Windows installer

#ifndef MyAppVersion
#define MyAppVersion "1.0.0"
#endif

#define MyAppName "Aitum Dock Sync"
#define MyAppPublisher "Captain Dark"
#ifndef PackageRoot
#define PackageRoot "..\release\Package"
#endif

[Setup]
AppId={{8F3A1C2E-9B47-4D60-A8E1-AITUMDOCKSYNC}
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
english.ObsCloseTitle=Before you continue
english.ObsCloseSubtitle=Close OBS Studio completely before installing.
english.ObsCloseBody=Aitum Dock Sync installs into your OBS Studio folder.%n%nClose OBS completely, including the system tray icon, before you click Next.%n%nOBS found at:%n%1
english.AfterInstall=Installation complete.%n%n1. Restart OBS Studio.%n2. Open Tools, Scripts and remove aitum_sync.py if it is still listed.%n3. Start streaming. Aitum Multistream outputs start and stop with your main stream.
english.UninstallObsRunning=OBS Studio is still running.%n%nClose OBS completely, including the system tray icon, then click Retry.%n%nClick Cancel to abort uninstall.
english.UninstallIncomplete=Some plugin files could not be removed. Close OBS and uninstall again, or delete them manually:%n%n%1

[Files]
Source: "{#PackageRoot}\obs-plugins\64bit\aitum-dock-sync.dll"; DestDir: "{code:GetObsPluginsDir}"; Flags: ignoreversion uninsneveruninstall; Check: ObsDirValid
Source: "{#PackageRoot}\data\obs-plugins\aitum-dock-sync\*"; DestDir: "{code:GetObsDataDir}"; Flags: ignoreversion recursesubdirs createallsubdirs uninsneveruninstall; Check: ObsDirValid

[Icons]
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Code]
var
  ObsDir: String;
  ObsClosePage: TWizardPage;
  ObsCloseLabel: TNewStaticText;

function ObsDirValid: Boolean;
begin
  Result := (ObsDir <> '') and DirExists(ObsDir);
end;

function GetObsPluginsDir(Param: String): String;
begin
  Result := ObsDir + '\obs-plugins\64bit';
end;

function GetObsDataDir(Param: String): String;
begin
  Result := ObsDir + '\data\obs-plugins\aitum-dock-sync';
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
    Answer := MsgBox(ExpandConstant('{cm:UninstallObsRunning}'), mbConfirmation, MB_RETRYCANCEL);
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

function RemoveInstalledFiles: Boolean;
var
  Failed: String;
  UserPlugin: String;
begin
  Result := True;
  Failed := '';

  ObsDir := FindObsInstall;
  if ObsDir <> '' then
  begin
    if not RemovePluginFile(ObsDir + '\obs-plugins\64bit\aitum-dock-sync.dll') then
      Failed := Failed + ObsDir + '\obs-plugins\64bit\aitum-dock-sync.dll' + #13#10;
    if not RemovePluginTree(ObsDir + '\data\obs-plugins\aitum-dock-sync') then
      Failed := Failed + ObsDir + '\data\obs-plugins\aitum-dock-sync' + #13#10;
  end;

  UserPlugin := ExpandConstant('{userappdata}\obs-studio\plugins\64bit\aitum-dock-sync.dll');
  if FileExists(UserPlugin) then
  begin
    if not DeleteFile(UserPlugin) then
      Failed := Failed + UserPlugin + #13#10;
  end;

  if Failed <> '' then
  begin
    MsgBox(FmtMessage(ExpandConstant('{cm:UninstallIncomplete}'), [Failed]), mbError, MB_OK);
    Result := False;
  end;
end;

function InitializeSetup: Boolean;
begin
  ObsDir := FindObsInstall;
  if ObsDir = '' then
  begin
    MsgBox(ExpandConstant('{cm:ObsNotFound}'), mbError, MB_OK);
    Result := False;
  end
  else
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
  if CurStep = ssPostInstall then
    MsgBox(ExpandConstant('{cm:AfterInstall}'), mbInformation, MB_OK);
end;

function InitializeUninstall: Boolean;
begin
  Result := WaitForObsClosed;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
    RemoveInstalledFiles;
end;
