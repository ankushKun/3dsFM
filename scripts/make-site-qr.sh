#!/usr/bin/env bash
# Regenerates the install QR codes on the website. They point at GitHub's
# "latest release" download URLs, so they only change if the repository or
# the asset names change.

set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO="ankushKun/3dsFM"
LATEST="https://github.com/${REPO}/releases/latest/download"
TOOL="$(mktemp -d)/qr-svg"
trap 'rm -rf "$(dirname "${TOOL}")"' EXIT

cc -O2 -I"${ROOT}/tests/third_party" "${ROOT}/scripts/qr-svg.c" \
    "${ROOT}/tests/third_party/qrcodegen.c" -o "${TOOL}"

"${TOOL}" "${LATEST}/3dsFM.cia" "${ROOT}/site/qr/install-cia.svg" \
    "QR code that installs the latest 3dsFM CIA with FBI"
"${TOOL}" "${LATEST}/3dsFM.3dsx" "${ROOT}/site/qr/download-3dsx.svg" \
    "QR code that downloads the latest 3dsFM.3dsx"

echo "Wrote site/qr/install-cia.svg and site/qr/download-3dsx.svg"
