; Inno Setup script for KaraSoft. Build with: ISCC.exe /DAppVersion=x.y.z installer\karasoft.iss
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif

[Setup]
AppId={{6B0C3B1E-6F2A-4C7E-9B7A-4B2C9E0F5A11}
AppName=KaraSoft
AppVersion={#AppVersion}
AppPublisher=KaraSoft
DefaultDirName={autopf}\KaraSoft
DefaultGroupName=KaraSoft
OutputBaseFilename=KaraSoft-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
ChangesAssociations=yes
UninstallDisplayIcon={app}\KaraSoft.exe

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "assocemk"; Description: "Open .emk files with KaraSoft"; GroupDescription: "File associations:"

[Files]
Source: "..\dist\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs; Excludes: "*.pdb"

[Icons]
Name: "{group}\KaraSoft"; Filename: "{app}\KaraSoft.exe"
Name: "{group}\{cm:UninstallProgram,KaraSoft}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\KaraSoft"; Filename: "{app}\KaraSoft.exe"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Classes\.emk\OpenWithProgids"; ValueType: string; ValueName: "KaraSoft.emk"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assocemk
Root: HKA; Subkey: "Software\Classes\KaraSoft.emk"; ValueType: string; ValueName: ""; ValueData: "EMK Karaoke"; Flags: uninsdeletekey; Tasks: assocemk
Root: HKA; Subkey: "Software\Classes\KaraSoft.emk\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\KaraSoft.exe"" ""%1"""; Tasks: assocemk

[Run]
Filename: "{app}\KaraSoft.exe"; Description: "{cm:LaunchProgram,KaraSoft}"; Flags: nowait postinstall skipifsilent
