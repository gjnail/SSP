# Packaging

This project includes packaging helpers for both Windows and macOS.

These scripts expect a configured build tree in `build-hyper/`, which is the verified build folder for this product scaffold.

## Windows

Run:

```powershell
.\scripts\build-windows-installer.ps1
```

Behavior:

- Builds the `VST3` and `Standalone` Release targets
- Optionally code-signs the plugin and installer if signing environment variables are set
- Uses Inno Setup to create a proper `.exe` installer if `ISCC.exe` is installed
- Falls back to an unsigned test `.zip` package if Inno Setup is not installed

## macOS

Run on a Mac:

```bash
./scripts/build-macos-installer.sh
```

Behavior:

- Builds the `VST3` and `Standalone` Release targets
- Creates a `.pkg` installer with `pkgbuild`/`productbuild` if those tools are available
- Falls back to a distributable `.zip` package if installer tools are unavailable
