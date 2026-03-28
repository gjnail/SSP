param([string]$Config = "Release")
$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot "build"
$artefactsDir = Join-Path $buildDir "SSPMinimize_artefacts\$Config"
$distDir = Join-Path $projectRoot "dist\windows"
$stageDir = Join-Path $distDir "stage"
function Invoke-Checked { param([string]$FilePath,[Parameter(ValueFromRemainingArguments = $true)][string[]]$Arguments) & $FilePath @Arguments; if ($LASTEXITCODE -ne 0) { throw "Command failed: $FilePath $($Arguments -join ' ')" } }
function Build-TargetOrUseExisting { param([string]$Target,[string]$ExpectedPath) & cmake --build $buildDir --config $Config --target $Target; if ($LASTEXITCODE -eq 0) { return }; if (Test-Path $ExpectedPath) { Write-Warning "Build target '$Target' reported an error, but the local artefact exists at '$ExpectedPath'. Continuing with the local build output."; return }; throw "Command failed: cmake --build $buildDir --config $Config --target $Target" }
function Get-ProjectVersion { $cmake = Get-Content (Join-Path $projectRoot "CMakeLists.txt"); $match = $cmake | Select-String 'project\(SSP_MINIMIZE VERSION ([0-9]+\.[0-9]+\.[0-9]+)'; if ($match) { return $match.Matches[0].Groups[1].Value }; return "0.1.0" }
function Find-InnoSetupCompiler { $command = Get-Command "ISCC.exe" -ErrorAction SilentlyContinue; if ($command) { return $command.Source }; foreach ($candidate in @("C:\Program Files (x86)\Inno Setup 6\ISCC.exe","C:\Program Files\Inno Setup 6\ISCC.exe")) { if (Test-Path $candidate) { return $candidate } }; return $null }
$version = Get-ProjectVersion
$vst3Path = Join-Path $artefactsDir "VST3\SSP Minimize.vst3"
$standalonePath = Join-Path $artefactsDir "Standalone\SSP Minimize.exe"
$zipPath = Join-Path $distDir "SSP-Minimize-Windows-Unsigned-Test-Package-$version.zip"
Build-TargetOrUseExisting -Target "SSPMinimize_VST3" -ExpectedPath $vst3Path
Build-TargetOrUseExisting -Target "SSPMinimize_Standalone" -ExpectedPath $standalonePath
if (-not (Test-Path $vst3Path)) { throw "Missing VST3 artefact at $vst3Path" }
if (-not (Test-Path $standalonePath)) { throw "Missing standalone artefact at $standalonePath" }
New-Item -ItemType Directory -Force -Path $distDir | Out-Null
if (Test-Path $stageDir) { Remove-Item -Recurse -Force $stageDir }
New-Item -ItemType Directory -Force -Path $stageDir | Out-Null
Copy-Item -Recurse -Force $vst3Path (Join-Path $stageDir "VST3")
New-Item -ItemType Directory -Force -Path (Join-Path $stageDir "Standalone") | Out-Null
Copy-Item -Force $standalonePath (Join-Path $stageDir "Standalone")
Copy-Item -Force (Join-Path $projectRoot "packaging\windows\installer-readme.txt") (Join-Path $stageDir "README.txt")
if (Test-Path $zipPath) { Remove-Item -Force $zipPath }
Compress-Archive -Path (Join-Path $stageDir "*") -DestinationPath $zipPath
$iscc = Find-InnoSetupCompiler
if ($iscc) { $env:SSP_PROJECT_ROOT = $projectRoot; Invoke-Checked $iscc "/DMyAppVersion=$version" "/DProjectRoot=$projectRoot" (Join-Path $projectRoot "packaging\windows\SSP-Minimize.iss"); Write-Host "Installer created in $distDir"; exit 0 }
Write-Host "Inno Setup not found. Created unsigned test ZIP package instead..."
