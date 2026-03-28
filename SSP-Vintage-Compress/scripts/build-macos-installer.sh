#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
ARTEFACTS_DIR="${BUILD_DIR}/SSPVintageCompress_artefacts/Release"
DIST_DIR="${PROJECT_ROOT}/dist/macos"
STAGE_DIR="${DIST_DIR}/stage"
PKG_ROOT="${DIST_DIR}/pkg-root"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "This script must be run on macOS."
  exit 1
fi

VERSION="$(sed -nE 's/^project\(SSP_VINTAGE_COMPRESS VERSION ([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' "${PROJECT_ROOT}/CMakeLists.txt" | head -n1)"
VERSION="${VERSION:-0.1.0}"

cmake --build "${BUILD_DIR}" --config Release --target SSPVintageCompress_VST3
cmake --build "${BUILD_DIR}" --config Release --target SSPVintageCompress_Standalone

VST3_PATH="${ARTEFACTS_DIR}/VST3/SSP Vintage Compress.vst3"
APP_PATH="${ARTEFACTS_DIR}/Standalone/SSP Vintage Compress.app"

if [[ ! -d "${VST3_PATH}" ]]; then
  echo "Missing VST3 artefact at ${VST3_PATH}"
  exit 1
fi

if [[ ! -d "${APP_PATH}" ]]; then
  echo "Missing standalone app at ${APP_PATH}"
  exit 1
fi

mkdir -p "${DIST_DIR}"
rm -rf "${STAGE_DIR}" "${PKG_ROOT}"
mkdir -p "${STAGE_DIR}/VST3" "${STAGE_DIR}/Applications"
cp -R "${VST3_PATH}" "${STAGE_DIR}/VST3/"
cp -R "${APP_PATH}" "${STAGE_DIR}/Applications/"
cp "${PROJECT_ROOT}/packaging/macos/installer-readme.txt" "${STAGE_DIR}/README.txt"

if command -v pkgbuild >/dev/null 2>&1 && command -v productbuild >/dev/null 2>&1; then
  mkdir -p "${PKG_ROOT}/Library/Audio/Plug-Ins/VST3" "${PKG_ROOT}/Applications"
  cp -R "${VST3_PATH}" "${PKG_ROOT}/Library/Audio/Plug-Ins/VST3/"
  cp -R "${APP_PATH}" "${PKG_ROOT}/Applications/"

  COMPONENT_PKG="${DIST_DIR}/SSP-Vintage-Compress-${VERSION}-component.pkg"
  FINAL_PKG="${DIST_DIR}/SSP-Vintage-Compress-macOS-Installer-${VERSION}.pkg"

  pkgbuild \
    --root "${PKG_ROOT}" \
    --identifier "com.ssp.vintagecompress.installer" \
    --version "${VERSION}" \
    --install-location "/" \
    "${COMPONENT_PKG}"

  productbuild \
    --package "${COMPONENT_PKG}" \
    "${FINAL_PKG}"

  echo "macOS installer created at ${FINAL_PKG}"
  exit 0
fi

ZIP_PATH="${DIST_DIR}/SSP-Vintage-Compress-macOS-Package-${VERSION}.zip"
rm -f "${ZIP_PATH}"
ditto -c -k --sequesterRsrc --keepParent "${STAGE_DIR}" "${ZIP_PATH}"
echo "pkgbuild/productbuild not found. Created ZIP package at ${ZIP_PATH}"
