; Inno Setup script for BrightKeys. Built by scripts\package.ps1:
;   ISCC.exe /DAppVersion=1.2.3 installer\brightkeys.iss
; Installs per-user by default (no admin prompt); an all-users install can be chosen in the dialog.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#define AppName "BrightKeys"
#define AppExe "BrightKeys.exe"
#define AppUrl "https://github.com/tejashwikalptaru/brightkeys"
#define RunKey "Software\Microsoft\Windows\CurrentVersion\Run"

[Setup]
AppId={{6F3C2B1E-8D4A-4C7B-9E21-B5A7D0F4C9E3}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Tejashwi Kalp Taru
AppPublisherURL=https://tejashwi.in
AppSupportURL={#AppUrl}/issues
AppUpdatesURL={#AppUrl}/releases
AppCopyright=Copyright (c) 2026 Tejashwi Kalp Taru
VersionInfoVersion={#AppVersion}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
DisableDirPage=auto
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
LicenseFile=..\LICENSE
SetupIconFile=..\res\brightkeys.ico
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName}
OutputDir=..\dist
OutputBaseFilename={#AppName}-{#AppVersion}-setup-x64
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=no

[Tasks]
Name: "autostart"; Description: "Start {#AppName} automatically when I sign in"; GroupDescription: "Startup:"

[Files]
Source: "..\build\{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"

[Registry]
Root: HKCU; Subkey: "{#RunKey}"; ValueType: string; ValueName: "{#AppName}"; ValueData: """{app}\{#AppExe}"""; Tasks: autostart; Flags: uninsdeletevalue

[Run]
Filename: "{app}\{#AppExe}"; Description: "Launch {#AppName} now"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{userappdata}\{#AppName}"

[Code]
const
  WM_CLOSE = $0010;

{ Ask a running instance to exit (its hidden main window has a fixed class name) and wait for it. }
procedure CloseRunningApp;
var
  Wnd: HWND;
  I: Integer;
begin
  Wnd := FindWindowByClassName('BrightKeysMain');
  if Wnd = 0 then Exit;
  PostMessage(Wnd, WM_CLOSE, 0, 0);
  for I := 1 to 30 do
  begin
    Sleep(100);
    if FindWindowByClassName('BrightKeysMain') = 0 then Exit;
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  CloseRunningApp;
  Result := '';
end;

{ Unchecking the startup task on an upgrade should remove a previously written value. }
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) and not WizardIsTaskSelected('autostart') then
    RegDeleteValue(HKEY_CURRENT_USER, '{#RunKey}', '{#AppName}');
end;

function InitializeUninstall(): Boolean;
begin
  CloseRunningApp;
  Result := True;
end;

{ The app itself can also create the Run value ("Start with Windows" menu item). }
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
    RegDeleteValue(HKEY_CURRENT_USER, '{#RunKey}', '{#AppName}');
end;
