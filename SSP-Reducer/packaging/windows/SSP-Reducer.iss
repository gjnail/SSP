#define MyAppName "SSP Reducer"
#define MyAppPublisher "Stupid Simple Plugins"
#ifndef MyAppVersion
  #define MyAppVersion "0.1.0"
#endif
#ifndef ProjectRoot
  #define ProjectRoot GetEnv("SSP_PROJECT_ROOT")
#endif
#define BuildArtefactsDir AddBackslash(ProjectRoot) + "build\\SSPReducer_artefacts\\Release"
#define StandaloneSource AddBackslash(BuildArtefactsDir) + "Standalone\\SSP Reducer.exe"
#define VST3Source AddBackslash(BuildArtefactsDir) + "VST3\\SSP Reducer.vst3"
#define ReadmeSource AddBackslash(ProjectRoot) + "packaging\\windows\\installer-readme.txt"
[Setup]
AppId={{31DDE6C1-7367-4532-B3CE-8C933D47D0D8}
AppName={#MyAppName}
AppVerName={#MyAppName} {#MyAppVersion}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf64}\Stupid Simple Plugins\SSP Reducer
DefaultGroupName=Stupid Simple Plugins
UninstallDisplayIcon={app}\SSP Reducer.exe
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
OutputDir={#BuildArtefactsDir}\..\..\dist\windows
OutputBaseFilename=SSP-Reducer-Windows-Installer-{#MyAppVersion}
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
DisableProgramGroupPage=yes
DisableDirPage=no
ShowComponentSizes=no
SetupLogging=yes
[Types]
Name: "full"; Description: "Plugin + standalone"
Name: "pluginonly"; Description: "Plugin only"
Name: "standaloneonly"; Description: "Standalone only"
[Components]
Name: "vst3"; Description: "VST3 plugin (recommended)"; Types: full pluginonly
Name: "standalone"; Description: "Standalone application"; Types: full standaloneonly
[Files]
Source: "{#StandaloneSource}"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone
Source: "{#ReadmeSource}"; DestDir: "{app}"; DestName: "README.txt"; Flags: ignoreversion; Components: standalone
Source: "{#VST3Source}\*"; DestDir: "{code:GetVST3InstallDir}"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3
[Run]
Filename: "{app}\SSP Reducer.exe"; Description: "Launch SSP Reducer"; Flags: nowait postinstall skipifsilent; Components: standalone
[Code]
var VST3DirPage: TInputDirWizardPage;
function DefaultVST3Dir: string;
begin
  Result := ExpandConstant('{commoncf64}\VST3\SSP Reducer.vst3');
end;
procedure InitializeWizard;
begin
  VST3DirPage := CreateInputDirPage(wpSelectDir, 'Choose VST3 Install Location', 'Select where SSP Reducer.vst3 should be installed.', 'The standard VST3 path is recommended, but you can choose a custom folder if your setup needs it.', False, '');
  VST3DirPage.Add('');
  VST3DirPage.Values[0] := DefaultVST3Dir();
end;
function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
  if (PageID = wpSelectDir) and (not WizardIsComponentSelected('standalone')) then Result := True
  else if (PageID = VST3DirPage.ID) and (not WizardIsComponentSelected('vst3')) then Result := True;
end;
function GetVST3InstallDir(Value: string): string;
begin
  if Assigned(VST3DirPage) and (VST3DirPage.Values[0] <> '') then Result := VST3DirPage.Values[0] else Result := DefaultVST3Dir();
end;
