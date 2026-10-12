#!/usr/bin/env bash
# Installs a pinned Azahar release into ~/Applications for scripts/run.sh.
# Azahar is the maintained continuation of Citra and is not on Homebrew, so
# the release zip is fetched from GitHub and checked against a known SHA-256.

set -Eeuo pipefail

VERSION="${AZAHAR_VERSION:-2126.2}"
ARCH="$(uname -m)"
case "${ARCH}" in
    arm64)  ASSET="azahar-macos-arm64-${VERSION}.zip"
            SHA256="c24dec2bd5111b7d79513d686c5aece6612cdbf9737b2e6ea99cfc426f1d4aef" ;;
    x86_64) ASSET="azahar-macos-x86_64-${VERSION}.zip"
            SHA256="${AZAHAR_SHA256:-}" ;;
    *)      echo "Unsupported architecture: ${ARCH}" >&2; exit 1 ;;
esac

URL="https://github.com/azahar-emu/azahar/releases/download/${VERSION}/${ASSET}"
DEST="${HOME}/Applications"
WORK="$(mktemp -d)"
trap 'rm -rf "${WORK}"' EXIT

echo "Downloading Azahar ${VERSION} (${ASSET})..."
curl -fsSL -o "${WORK}/azahar.zip" "${URL}"
if [[ -n "${SHA256}" ]]; then
    echo "${SHA256}  ${WORK}/azahar.zip" | shasum -a 256 -c -
else
    echo "No pinned checksum for ${ARCH}; set AZAHAR_SHA256 to verify." >&2
fi

unzip -q "${WORK}/azahar.zip" -d "${WORK}/unpacked"
APP="$(find "${WORK}/unpacked" -maxdepth 3 -name 'Azahar.app' | head -n 1)"
[[ -n "${APP}" ]] || { echo "Azahar.app not found in the archive." >&2; exit 1; }

mkdir -p "${DEST}"
rm -rf "${DEST}/Azahar.app"
ditto "${APP}" "${DEST}/Azahar.app"
xattr -dr com.apple.quarantine "${DEST}/Azahar.app" 2>/dev/null || true
echo "Installed ${DEST}/Azahar.app"
