# Packaging

This project includes packaging helpers for both Windows and macOS.

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

Optional signing environment variables:

```powershell
$env:SSP_SIGN_CERT_PFX="C:\path\to\certificate.pfx"
$env:SSP_SIGN_CERT_PASSWORD="your-password"
$env:SSP_SIGN_TIMESTAMP_URL="http://timestamp.digicert.com"
```

Recommended distribution:

- Signed Inno Setup installer for end users
- Unsigned test zip only for trusted testers

## macOS

Run on a Mac:

```bash
./scripts/build-macos-installer.sh
```

Behavior:

- Builds the `VST3` and `Standalone` Release targets
- Creates a `.pkg` installer with `pkgbuild`/`productbuild` if those tools are available
- Falls back to a distributable `.zip` package if installer tools are unavailable
- Uses the SSP Limiter build artefacts and product naming
