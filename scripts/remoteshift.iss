; RemoteShift Windows installer (Inno Setup 6).
; Built by .github/workflows/remoteshift-windows.yml:  ISCC /DMyAppVersion=0.1.0 scripts\remoteshift.iss

#define MyAppName "RemoteShift"
#define MyAppPublisher "RemoteShift contributors"
#define MyAppExeName "RemoteShift.exe"
#define MyAppPath "..\RemoteShift-Win"
#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif

[Setup]
; Unique to RemoteShift: installs side by side with upstream chiaki-ng.
AppId={{6B1E0C47-3F2D-4C8E-9A51-7D2F5E8B4A19}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
VersionInfoVersion={#MyAppVersion}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Per-user install by default: no administrator rights required.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
AllowNoIcons=yes
LicenseFile=..\LICENSES\AGPL-3.0-only-OpenSSL.txt
OutputBaseFilename=RemoteShift-{#MyAppVersion}-win64-setup
OutputDir=..
SetupIconFile=..\gui\remoteshift.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#MyAppPath}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
