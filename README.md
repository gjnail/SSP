# SSP

This repository contains the Stupid Simple Plugins workspace: a collection of JUCE/CMake audio plugins, plus the supporting desktop hub, website, presets, and packaging scripts.

The repo is organized as independent projects. Each plugin has its own `CMakeLists.txt`, `Source/`, `scripts/`, and `packaging/` folder. There is not currently a single top-level superbuild for every plugin at once.

## Repository layout

- `Simple-Sidechain` through `SSP-Vintage-Compress`: JUCE audio plugins with VST3 and Standalone targets
- `SSP-Hub`: Electron desktop app for installs and activation
- `SSP-Website`: React/Vite website
- `Reactor Presets`: preset assets for SSP Reactor
- `Installers`: generated packaging output and release handoff files
- `cmake/SSPJuce.cmake`: shared JUCE bootstrap used by the plugin projects

## Prerequisites

### For the JUCE plugins

- Git
- CMake 3.22 or newer
- A C++17-capable toolchain
- Internet access on first configure unless JUCE is already available locally

Windows:

- Visual Studio 2022 or Build Tools 2022 with Desktop C++ workload
- Inno Setup 6 if you want `.exe` installers instead of test ZIP packages

macOS:

- Xcode or Xcode Command Line Tools
- `pkgbuild` and `productbuild` if you want `.pkg` installers instead of ZIP packages

### For the apps

- Node.js 20+ and npm

## JUCE dependency resolution

Each plugin project now resolves JUCE in this order:

1. `-DSSP_JUCE_PATH=/absolute/path/to/JUCE`
2. `third_party/JUCE` inside this repo
3. `SSP-Multichain/build/_deps/juce-src` if it already exists locally
4. Fetch JUCE `7.0.12` from GitHub with CMake `FetchContent`

That means a fresh clone can build on another machine without editing any personal file paths.

## Building the audio plugins

From inside any plugin directory:

```powershell
cmake -S . -B build
cmake --build build --config Release --target <TargetName>_VST3
cmake --build build --config Release --target <TargetName>_Standalone
```

If you prefer a single-config generator such as Ninja, configure with:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target <TargetName>_VST3
cmake --build build --target <TargetName>_Standalone
```

The packaging scripts expect that `build/` already exists, so run the CMake configure step at least once before packaging.

### Installer packaging

Windows, from inside a plugin folder:

```powershell
.\scripts\build-windows-installer.ps1
```

Behavior:

- Builds the Release `VST3` and `Standalone` targets
- Produces a staged package under `dist/windows`
- Uses Inno Setup when available
- Falls back to an unsigned ZIP package when Inno Setup is not installed

Optional Windows signing environment variables used by several projects:

```powershell
$env:SSP_SIGN_CERT_PFX="C:\path\to\certificate.pfx"
$env:SSP_SIGN_CERT_PASSWORD="your-password"
$env:SSP_SIGN_TIMESTAMP_URL="http://timestamp.digicert.com"
```

macOS, from inside a plugin folder:

```bash
./scripts/build-macos-installer.sh
```

Behavior:

- Builds the Release `VST3` and `Standalone` targets
- Produces a staged package under `dist/macos`
- Uses `pkgbuild` and `productbuild` when available
- Falls back to a ZIP package when Apple installer tools are not available

## Product matrix

| Folder | Product | What it is | Build targets | Installer automation |
| --- | --- | --- | --- | --- |
| `Simple-Sidechain` | SSP Simple Sidechain | Sidechain / pump processor with audio, BPM-sync, and MIDI triggering | `SSPSimpleSidechain_VST3`, `SSPSimpleSidechain_Standalone` | Windows + macOS |
| `SSP-3OSC` | SSP Reactor | 3-oscillator synth with modulation, presets, and a full effects rack | `SSP3OSC_VST3`, `SSP3OSC_Standalone` | Windows only |
| `SSP-Beef` | SSP Beef | One-knob saturation and compression thickener | `SSPBeef_VST3`, `SSPBeef_Standalone` | Windows only |
| `SSP-Clipper` | SSP Clipper | Clipping / loudness shaping processor | `SSPClipper_VST3`, `SSPClipper_Standalone` | Windows + macOS |
| `SSP-Comb` | SSP Comb | Comb-filter style resonance and tone effect | `SSPComb_VST3`, `SSPComb_Standalone` | Windows + macOS |
| `SSP-Delay` | SSP Delay | Delay effect | `SSPDelay_VST3`, `SSPDelay_Standalone` | Windows + macOS |
| `SSP-Echo` | SSP Echo | Echo / ambience effect | `SSPEcho_VST3`, `SSPEcho_Standalone` | Windows + macOS |
| `SSP-EQ` | SSP EQ | Equalizer plugin | `SSPEQ_VST3`, `SSPEQ_Standalone` | Windows + macOS |
| `SSP-Hihat-God` | SSP Hihat God | Tempo-synced movement tool for hats and other rhythmic parts | `SSPHihatGod_VST3`, `SSPHihatGod_Standalone` | Windows + macOS |
| `SSP-Loud` | SSP Loud | LUFS and peak metering tool | `SSPLoud_VST3`, `SSPLoud_Standalone` | Windows + macOS |
| `SSP-Minimize` | SSP Minimize | Dynamic harshness / resonance reduction processor | `SSPMinimize_VST3`, `SSPMinimize_Standalone` | Windows only |
| `SSP-Multichain` | SSP Multiband Sidechain | Multiband sidechain ducking processor | `SSPMultichain_VST3`, `SSPMultichain_Standalone` | Windows + macOS |
| `SSP-Reducer` | SSP Reducer | Bitcrusher and sample-rate reduction effect | `SSPReducer_VST3`, `SSPReducer_Standalone` | Windows + macOS |
| `SSP-Reverb` | SSP Reverb | Reverb effect | `SSPReverb_VST3`, `SSPReverb_Standalone` | Windows + macOS |
| `SSP-Saturate` | SSP Saturate | Saturation / drive processor | `SSPSaturate_VST3`, `SSPSaturate_Standalone` | Windows + macOS |
| `SSP-Sub-Validator` | SSP Sub Validator | Low-end spectrum validator and sub-checking tool | `SSPSubValidator_VST3`, `SSPSubValidator_Standalone` | Windows + macOS |
| `SSP-Transient-Control` | SSP Transient Control | Transient shaping processor | `SSPTransientControl_VST3`, `SSPTransientControl_Standalone` | Windows + macOS |
| `SSP-Transition` | SSP Transition | One-knob transition FX tool with filter, width, and reverb presets | `SSPTransition_VST3`, `SSPTransition_Standalone` | Windows + macOS |
| `SSP-Vintage-Compress` | SSP Vintage Compress | Vintage-style compressor | `SSPVintageCompress_VST3`, `SSPVintageCompress_Standalone` | Windows + macOS |

## SSP Hub

`SSP-Hub` is the Electron desktop app for installs and activation.

Setup and run:

```powershell
cd SSP-Hub
npm install
npm run dev
```

Build:

```powershell
npm run build
```

Package:

```powershell
npm run package:win
npm run package:mac
```

## SSP Website

`SSP-Website` is the React/Vite storefront and marketing site.

Setup and run:

```powershell
cd SSP-Website
npm install
npm run dev
```

Build:

```powershell
npm run build
```

Preview production build:

```powershell
npm run preview
```

## Suggested first-time repo setup

From the root of the `SSP` folder:

```powershell
git init -b main
git add .
git commit -m "Initial import"
```

To publish it to GitHub after creating an empty remote:

```powershell
git remote add origin https://github.com/<your-user>/<your-repo>.git
git push -u origin main
```

## Notes

- The root `.gitignore` is set up to keep generated build folders, packaged installers, `node_modules`, and local scratch exports out of source control.
- A few products currently have Windows packaging automation only: `SSP-3OSC`, `SSP-Beef`, and `SSP-Minimize`.
- The plugin projects are independent, so build and package them from within each project directory.
