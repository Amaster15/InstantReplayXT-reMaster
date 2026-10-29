#ifndef PackageDir
  #error PackageDir is required
#endif
#ifndef OutputDir
  #error OutputDir is required
#endif
[Setup]
AppId={{6D1D9A87-2C13-4BE8-A2B7-F0E2281C8935}
AppName=Instant Replay XT for OBS
AppVersion=2.0.14
AppPublisher=InstantReplayXT-reMaster contributors
AppPublisherURL=https://github.com/Amaster15/InstantReplayXT-reMaster
DefaultDirName={commonappdata}\obs-studio\plugins\InstantReplay
DisableDirPage=no
UsePreviousAppDir=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputDir}
OutputBaseFilename=InstantReplayXT-2.0.14-obs31-windows-x64-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=Instant Replay XT for OBS
UninstallFilesDir={app}\uninstall
CloseApplications=yes
RestartApplications=no
InfoBeforeFile=windows-notice.txt

[Messages]
SelectDirLabel3=Choose the InstantReplay plugin folder. Keep the default for standard OBS. A custom folder must be in a plugin search location configured in OBS; this does not choose where video clips are saved.

[Files]
Source: "{#PackageDir}\InstantReplay\bin\64bit\InstantReplay.dll"; DestDir: "{app}\bin\64bit"; Flags: ignoreversion
Source: "{#PackageDir}\InstantReplay\data\locale\en-US.ini"; DestDir: "{app}\data\locale"; Flags: ignoreversion
Source: "{#PackageDir}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PackageDir}\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PackageDir}\build-info.json"; DestDir: "{app}"; Flags: ignoreversion

Source: "{#PackageDir}\InstantReplay\data\updater\*"; DestDir: "{app}\data\updater"; Flags: ignoreversion

[Code]
function OBSRunning(): Boolean;
var Locator, Services, Processes: Variant;
begin
  Locator := CreateOleObject('WbemScripting.SWbemLocator');
  Services := Locator.ConnectServer('.', 'root\CIMV2');
  Processes := Services.ExecQuery('SELECT ProcessId FROM Win32_Process WHERE Name = ''obs64.exe''');
  Result := Processes.Count > 0;
end;

function InitializeSetup(): Boolean;
begin
  Result := False;
  try
    if OBSRunning() then begin
      MsgBox('Close OBS Studio before installing Instant Replay XT, then run this installer again.', mbError, MB_OK);
      Exit;
    end;
  except
    MsgBox('Could not check whether OBS is running. Windows Management Instrumentation must be available before setup can continue.', mbError, MB_OK);
    Exit;
  end;
  if FileExists(ExpandConstant('{autopf}\obs-studio\obs-plugins\64bit\InstantReplay.dll')) then begin
    MsgBox('An old InstantReplay.dll exists in the OBS application folder. Move it out of obs-plugins\64bit first to prevent OBS loading two copies, then run setup again.', mbError, MB_OK);
    Exit;
  end;
  Result := True;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var OldFile, BackupDir: String;
begin
  Result := '';
  if CompareText(ExtractFileName(RemoveBackslashUnlessRoot(ExpandConstant('{app}'))), 'InstantReplay') <> 0 then begin
    Result := 'Choose a dedicated folder named InstantReplay, inside an OBS plugin search folder.';
    Exit;
  end;
  if (CompareText(ExpandConstant('{app}'), ExpandConstant('{commonappdata}\obs-studio\plugins\InstantReplay')) <> 0) and
     FileExists(ExpandConstant('{commonappdata}\obs-studio\plugins\InstantReplay\bin\64bit\InstantReplay.dll')) then begin
    Result := 'A copy is installed in the standard OBS plugin folder. Uninstall it before moving to a different folder to avoid loading two copies. OBS settings and clips are preserved.';
    Exit;
  end;
  if OBSRunning() then begin
    Result := 'Close OBS Studio before continuing.';
    Exit;
  end;
  OldFile := ExpandConstant('{app}\bin\64bit\InstantReplay.dll');
  if FileExists(OldFile) then begin
    BackupDir := ExpandConstant('{commonappdata}\obs-studio\plugin-backups\InstantReplay-') + GetDateTimeString('yyyymmdd-hhnnss-zzz', '-', ':');
    if not ForceDirectories(BackupDir) then begin
      Result := 'Could not create the existing-plugin backup folder.';
      Exit;
    end;
    if not FileCopy(OldFile, BackupDir + '\InstantReplay.dll', True) then
      Result := 'Could not back up the existing plugin. Close OBS and try again.';
  end;
end;

function InitializeUninstall(): Boolean;
begin
  Result := False;
  try
    if OBSRunning() then begin
      MsgBox('Close OBS Studio before uninstalling Instant Replay XT.', mbError, MB_OK);
      Exit;
    end;
  except
    MsgBox('Could not check whether OBS is running. Close OBS and restore Windows Management Instrumentation before uninstalling.', mbError, MB_OK);
    Exit;
  end;
  Result := True;
end;
