#!/usr/bin/env bash
# Reproducible container build: 3DSX for the Homebrew Launcher plus a CIA for
# the HOME Menu. Needs Docker (Docker Desktop, or the CLI with Colima).
#
#   ./scripts/build.sh                 full build into dist/
#   THREEDSFM_3DSX_ONLY=1 ./scripts/build.sh   skip the CIA (fast dev loop)

set -Eeuo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
# Pinned by digest so a re-pushed tag cannot silently change the toolchain.
BUILD_IMAGE="${THREEDSFM_BUILD_IMAGE:-devkitpro/devkitarm:20260610@sha256:116afba8df8453961de2936ffab20dd441edf4d682856c1ec8b0e53d7ed0bbf5}"
CIA_IMAGE="${THREEDSFM_CIA_IMAGE:-3dsfm-cia-tools:1.0.0}"
BUILD_JOBS="${THREEDSFM_BUILD_JOBS:-4}"
ONLY_3DSX="${THREEDSFM_3DSX_ONLY:-0}"
KEEP_COLIMA="${THREEDSFM_KEEP_COLIMA:-0}"
DIST_DIR="${PROJECT_ROOT}/dist"
STARTED_COLIMA=0
STEP=0

step() { STEP=$((STEP + 1)); printf '\n[%d] %s\n' "${STEP}" "$1"; }
fail() { printf '\nBuild failed: %s\n' "$1" >&2; exit 1; }

cleanup() {
    if [[ "${STARTED_COLIMA}" == "1" && "${KEEP_COLIMA}" != "1" ]]; then
        printf '\nStopping the Colima instance started by this script...\n'
        colima stop >/dev/null
    fi
}
trap cleanup EXIT

DOCKER_USER="$(id -u):$(id -g)"

run_builder() {
    docker run --rm --user "${DOCKER_USER}" --env HOME=/tmp \
        --volume "${PROJECT_ROOT}:/project" --workdir /project \
        "${BUILD_IMAGE}" "$@"
}

run_cia_tool() {
    docker run --rm --platform linux/amd64 --user "${DOCKER_USER}" \
        --volume "${PROJECT_ROOT}:/project" --workdir /project \
        "${CIA_IMAGE}" "$@"
}

[[ "${BUILD_JOBS}" =~ ^[1-9][0-9]*$ ]] || fail "THREEDSFM_BUILD_JOBS must be a positive integer."

VERSION="$(tr -d '[:space:]' < "${PROJECT_ROOT}/VERSION")"
[[ "${VERSION}" =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)$ ]] || \
    fail "VERSION must look like MAJOR.MINOR.PATCH, found '${VERSION}'."
VERSION_MAJOR="${BASH_REMATCH[1]}"
VERSION_MINOR="${BASH_REMATCH[2]}"
VERSION_MICRO="${BASH_REMATCH[3]}"
printf 'Building 3dsFM %s\n' "${VERSION}"

step "Checking the container tools"
command -v docker >/dev/null 2>&1 || fail "Docker is required. Install Docker Desktop or Docker CLI with Colima."
if ! docker info >/dev/null 2>&1; then
    command -v colima >/dev/null 2>&1 || fail "Docker is not running and Colima is not installed."
    printf 'Docker is stopped; starting Colima...\n'
    colima start
    STARTED_COLIMA=1
    docker info >/dev/null 2>&1 || fail "Docker did not become available after Colima started."
fi

step "Checking the pinned devkitPro image"
if ! docker image inspect "${BUILD_IMAGE}" >/dev/null 2>&1; then
    docker pull "${BUILD_IMAGE}"
else
    printf 'Using the locally cached image.\n'
fi

step "Checking the artwork"
ICON="${PROJECT_ROOT}/resources/icon-3dsfm.png"
if [[ ! -f "${ICON}" || ! -f "${PROJECT_ROOT}/resources/banner-3dsfm.png" ]]; then
    printf 'Generating placeholder artwork...\n'
    python3 -I "${SCRIPT_DIR}/make-art.py"
fi
PNG_COLOR_TYPE="$(od -An -t u1 -j 25 -N 1 "${ICON}" | tr -d '[:space:]')"
[[ "${PNG_COLOR_TYPE}" == "4" || "${PNG_COLOR_TYPE}" == "6" ]] || \
    fail "The launcher PNG must contain a real alpha channel (PNG color type 4 or 6)."

step "Cleaning previous build outputs"
run_builder make clean

step "Compiling 3dsFM with ${BUILD_JOBS} job(s)"
run_builder make -j"${BUILD_JOBS}"

mkdir -p "${DIST_DIR}"
cp "${PROJECT_ROOT}/3dsFM.3dsx" "${DIST_DIR}/3dsFM.3dsx"
cp "${PROJECT_ROOT}/3dsFM.smdh" "${DIST_DIR}/3dsFM.smdh"

if [[ "${ONLY_3DSX}" == "1" ]]; then
    printf '\n3DSX build of 3dsFM %s complete: %s\n' "${VERSION}" "${DIST_DIR}/3dsFM.3dsx"
    exit 0
fi

step "Checking the pinned CIA packaging image (${CIA_IMAGE})"
if ! docker image inspect "${CIA_IMAGE}" >/dev/null 2>&1; then
    docker build --platform linux/amd64 \
        --file "${PROJECT_ROOT}/scripts/cia-tools.Dockerfile" \
        --tag "${CIA_IMAGE}" "${PROJECT_ROOT}"
else
    printf 'Using the locally cached CIA packaging image.\n'
fi

step "Creating HOME Menu metadata and banner"
run_cia_tool bannertool makebanner \
    -i resources/banner-3dsfm.png \
    -a resources/banner/jingle.wav \
    -o build/3dsFM.bnr
run_cia_tool bannertool makesmdh \
    -s "3dsFM" \
    -l "Your music, in stereo, on the 3DS" \
    -p "ankushKun" \
    -i resources/icon-3dsfm.png \
    -f visible,nosavebackups \
    -o build/3dsFM.icn

step "Packaging the installable CIA"
run_cia_tool makerom \
    -f cia -o 3dsFM.cia -target t -exefslogo \
    -elf 3dsFM.elf \
    -rsf resources/3dsFM.rsf \
    -banner build/3dsFM.bnr \
    -icon build/3dsFM.icn \
    -major "${VERSION_MAJOR}" -minor "${VERSION_MINOR}" -micro "${VERSION_MICRO}"

step "Validating build artifacts"
for artifact in 3dsFM.3dsx 3dsFM.smdh 3dsFM.cia; do
    [[ -s "${PROJECT_ROOT}/${artifact}" ]] || fail "Missing or empty artifact: ${artifact}"
    printf 'Found %s\n' "${artifact}"
done

step "Packaging dist/ and generating SHA-256"
cp "${PROJECT_ROOT}/3dsFM.cia" "${DIST_DIR}/3dsFM.cia"
(
    cd "${DIST_DIR}"
    if command -v shasum >/dev/null 2>&1; then
        shasum -a 256 3dsFM.3dsx 3dsFM.cia > 3dsFM.sha256
    else
        sha256sum 3dsFM.3dsx 3dsFM.cia > 3dsFM.sha256
    fi
)

step "Removing intermediate build files"
run_builder make clean

printf '\nBuild of 3dsFM %s complete.\n' "${VERSION}"
printf '  3DSX:     %s\n' "${DIST_DIR}/3dsFM.3dsx"
printf '  CIA:      %s\n' "${DIST_DIR}/3dsFM.cia"
printf '  Metadata: %s\n' "${DIST_DIR}/3dsFM.smdh"
printf '  Checksum: %s\n' "${DIST_DIR}/3dsFM.sha256"
