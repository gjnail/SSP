# Packaging

`SSP Spectrum` currently builds the same JUCE targets as the other SSP projects:

- `SSPSpectrum_VST3`
- `SSPSpectrum_Standalone`

Configure and build from the project folder:

```bash
/Users/hiimghost/Library/Python/3.9/bin/cmake -S . -B build
/Users/hiimghost/Library/Python/3.9/bin/cmake --build build --config Release --target SSPSpectrum_VST3
/Users/hiimghost/Library/Python/3.9/bin/cmake --build build --config Release --target SSPSpectrum_Standalone
```

If you want full installer automation later, the closest existing references in this repo are:

- [`SSP-EQ/scripts/build-macos-installer.sh`](/Users/hiimghost/Documents/SSP/SSP-EQ/scripts/build-macos-installer.sh)
- [`SSP-EQ/scripts/build-windows-installer.ps1`](/Users/hiimghost/Documents/SSP/SSP-EQ/scripts/build-windows-installer.ps1)
