#define MyAppName "SSP Beef"
#define MyAppPublisher "Stupid Simple Plugins"
#ifndef MyAppVersion
  #define MyAppVersion "0.1.0"
#endif
#ifndef ProjectRoot
  #define ProjectRoot GetEnv("SSP_PROJECT_ROOT")
#endif

#define BuildArtefactsDir AddBackslash(ProjectRoot) + "build\\SSPBeef_artefacts\\Release"
#define StandaloneSource AddBackslash(BuildArtefactsDir) + "Standalone\\SSP Beef.exe"
#define VST3Source AddBackslash(BuildArtefactsDir) + "VST3\\SSP Beef.vst3"
#define ReadmeSource AddBackslash(ProjectRoot) + "packaging\\windows\\installer-readme.txt"

[Setup]
AppId={{A2E9186D-8A97-4D6D-94A6-55F0AC1E5AF4}
AppName={#MyAppName}
AppVerName={#MyAppName} {#MyAppVersion}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf64}\Stupid Simple Plugins\SSP Beef
DefaultGroupName=Stupid Simple Plugins
UninstallDisplayIcon={app}\SSP Beef.exe
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
OutputDir={#BuildArtefactsDir}\..\..\dist\windows
OutputBaseFilename=SSP-Beef-Windows-Installer-{#MyAppVersion}
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
DisableProgramGroupPage=yes
DisableDirPage=no
DirExistsWarning=no
ShowComponentSizes=no
ChangesEnvironment=no
ChangesAssociations=no
SetupLogging=yes

[Types]
Name: "full"; Description: "Plugin + standalone"
Name: "pluginonly"; Description: "Plugin only"
Name: "standaloneonly"; Description: "Standalone only"

[Components]
Name: "vst3"; Description: "VST3 plugin (recommended)"; Types: full pluginonly
Name: "standalone"; Description: "Standalone application"; Types: full standaloneonly

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut for the standalone app"; Components: standalone

[Files]
Source: "{#StandaloneSource}"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone
Source: "{#ReadmeSource}"; DestDir: "{app}"; DestName: "README.txt"; Flags: ignoreversion; Components: standalone
Source: "{#VST3Source}\*"; DestDir: "{code:GetVST3InstallDir}"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3

[Icons]
Name: "{group}\SSP Beef"; Filename: "{app}\SSP Beef.exe"; Components: standalone
Name: "{autodesktop}\SSP Beef"; Filename: "{app}\SSP Beef.exe"; Tasks: desktopicon; Components: standalone
Name: "{group}\Uninstall SSP Beef"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\SSP Beef.exe"; Description: "Launch SSP Beef"; Flags: nowait postinstall skipifsilent; Components: standalone

[Messages]
WizardSelectComponents=Choose which parts of SSP Beef to install.
SelectDirDesc=Choose where to install the standalone application.
SelectDirLabel3=Choose where to install the standalone application files.

[CustomMessages]
VST3DirPageTitle=Choose VST3 Install Location
VST3DirPageSubTitle=Select where SSP Beef.vst3 should be installed.
VST3DirPageDescription=The standard VST3 path is recommended, but you can choose a custom folder if your setup needs it.

[Code]
var
  VST3DirPage: TInputDirWizardPage;

function DefaultVST3Dir: string;
begin
  Result := ExpandConstant('{commoncf64}\VST3\SSP Beef.vst3');
end;

procedure InitializeWizard;
begin
  VST3DirPage := CreateInputDirPage(
    wpSelectDir,
    ExpandConstant('{cm:VST3DirPageTitle}'),
    ExpandConstant('{cm:VST3DirPageSubTitle}'),
    ExpandConstant('{cm:VST3DirPageDescription}'),
    False,
    ''
  );
  VST3DirPage.Add('');
  VST3DirPage.Values[0] := DefaultVST3Dir();
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;

  if (PageID = wpSelectDir) and (not WizardIsComponentSelected('standalone')) then
    Result := True
  else if (PageID = VST3DirPage.ID) and (not WizardIsComponentSelected('vst3')) then
    Result := True;
end;

function GetVST3InstallDir(Value: string): string;
begin
  if Assigned(VST3DirPage) and (VST3DirPage.Values[0] <> '') then
    Result := VST3DirPage.Values[0]
  else
    Result := DefaultVST3Dir();
end;
