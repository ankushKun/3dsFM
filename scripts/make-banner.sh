#!/usr/bin/env bash
# Rebuilds 3dsFM's 3D HOME Menu banner from resources/banner/:
#   jingle.py      -> jingle.wav    (the sound played when the title is selected)
#   model.py       -> banner.glb    (the animated model, from logo.png)
#   build-cgfx.py  -> 3dsFM.cgfx    (what bannertool packs into the banner)
#
# The outputs are committed, so scripts/build.sh never needs this; run it
# after changing anything in resources/banner/. The CGFX step runs pycgfx in
# a pinned Python container, with pycgfx's files fetched at a pinned commit
# and checked against known hashes (pycgfx has no licence file, so it is
# fetched to a temporary folder rather than kept in this repository).
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BANNER="${PROJECT_ROOT}/resources/banner"
PYTHON_IMAGE="python:3.12-slim@sha256:a6e34c598f2467ed0e9a8d349809fcd8b5c603269512df273a0bb1784edc11b1"
PYCGFX_COMMIT="1f78850086f3a77c41e07162e842f97a5bf3c18a"
PYCGFX_FILES=(
    "8ed86c8f403f76adea97859d9ef1cf3689013617f87b31e8e8ccb763e4b9b896 main.py"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 cgfx/__init__.py"
    "b14e0734e6a371fb83c8767b81603f60bc01f661ccbe2030f49771432fc00966 cgfx/animation.py"
    "f746dafb0b133bc0114fc8f3c6fdcdca452c34fd5c2876ddd5de70d16cc1cd07 cgfx/canm.py"
    "f43021ecc762b499cc8bed5bbf02edadb56b983c80b9e23322f847ddda3b15aa cgfx/cenv.py"
    "587260228d6f002ea58a236d37a9772bf0227c5d7b5f266276f0340794882dff cgfx/cflt.py"
    "1b009a15600e62ad20c503206817bc101585441926c598bf25b5791fefa633fe cgfx/cgfx.py"
    "b75116d48fc0b72bbed69b9bc5c2a4fbe304d578dfe0e2a1fc5a50f84836c65c cgfx/cmdl.py"
    "d29cea0abdb3735de2f189b0d4b2a3c02d75eb1184c936049c37bf73d75d921a cgfx/dict.py"
    "08fbef437c7f3b729906cd01895ce0695ee126eadef82531b4dd1346e1a3a10f cgfx/luts.py"
    "4eecd2abc1015ad15b85f06f7cf00e04b478f746973dc0708b259105f01e4d39 cgfx/mtob.py"
    "bf120761d281733a7c1e02ef2f805e7174fb95b051766a0d3ee11d6cb8dfaa49 cgfx/patricia.py"
    "c6b475be1392c8a1cfc0bb5c8ddf1ab1a180759e2a67fca25fe89611b04027e3 cgfx/primitives.py"
    "a5f570c9a17482f52b125bd77a19f9cc35fa9ba2bd151d114888cec2583e0add cgfx/shared.py"
    "464250500e96c1178d8ca66a791d961354224b0d2880b9f58661626d8498a6f7 cgfx/sobj.py"
    "f63bd6e163a3a70342b40a2f041e7aad4c2693bddcf0843a10d1112a85353389 cgfx/swizzler.py"
    "8b1aae4afd9337f234493800a84b77f50897706dc436d648fa86aaf01c82b9e9 cgfx/txob.py"
)
# pycgfx's own Pipfile.lock at that commit.
PYTHON_PACKAGES="dataclasses-json==0.6.7 gltflib==1.0.13 marshmallow==3.26.1 mypy-extensions==1.1.0 packaging==25.0 pillow==11.2.1 typing-extensions==4.13.2 typing-inspect==0.9.0"

step() { printf '\n==> %s\n' "$1"; }
fail() { printf 'error: %s\n' "$1" >&2; exit 1; }

command -v python3 >/dev/null 2>&1 || fail "python3 is required."
command -v docker >/dev/null 2>&1 || fail "Docker is required for the CGFX step."
docker info >/dev/null 2>&1 || fail "Docker is not running."

# Under build/ (ignored by git) rather than the system temp folder, which
# Docker Desktop on macOS does not share with containers.
mkdir -p "${PROJECT_ROOT}/build"
WORK="$(mktemp -d "${PROJECT_ROOT}/build/banner.XXXXXX")"
trap 'rm -rf "${WORK}"' EXIT

step "Synthesising the jingle"
python3 "${BANNER}/jingle.py" "${BANNER}/jingle.wav"

step "Building the model"
python3 "${BANNER}/model.py" "${BANNER}/logo.png" "${BANNER}/banner.glb"

step "Fetching pycgfx ${PYCGFX_COMMIT:0:10}"
for entry in "${PYCGFX_FILES[@]}"; do
    hash="${entry%% *}"
    file="${entry#* }"
    mkdir -p "${WORK}/pycgfx/$(dirname "${file}")"
    curl -fsSL "https://raw.githubusercontent.com/skyfloogle/pycgfx/${PYCGFX_COMMIT}/${file}" -o "${WORK}/pycgfx/${file}"
    actual="$(shasum -a 256 "${WORK}/pycgfx/${file}" | cut -d' ' -f1)"
    [[ "${actual}" == "${hash}" ]] || fail "pycgfx ${file} does not match its pinned hash."
done

# THREEDSFM_BANNER_STILL=<seconds> also writes build/3dsFM-still.cgfx, one
# moment of the scene with no animation, so a test build on a console can
# tell an animation problem from anything else:
#   THREEDSFM_BANNER_CGFX=build/3dsFM-still.cgfx ./scripts/build.sh
STILL="${THREEDSFM_BANNER_STILL:-}"
CONVERT="python3 build-cgfx.py /pycgfx banner.glb 3dsFM.cgfx"
if [[ -n "${STILL}" ]]; then
    step "Building a still of the model at ${STILL} s"
    python3 "${BANNER}/model.py" --static "${STILL}" "${BANNER}/logo.png" "${WORK}/still.glb"
    CONVERT="${CONVERT} && python3 build-cgfx.py /pycgfx /work/still.glb /work/3dsFM-still.cgfx"
fi

step "Converting to CGFX"
docker run --rm \
    --volume "${BANNER}:/banner" \
    --volume "${WORK}:/work" \
    --volume "${WORK}/pycgfx:/pycgfx:ro" \
    --workdir /banner \
    "${PYTHON_IMAGE}" \
    sh -c "pip install --quiet --no-cache-dir --disable-pip-version-check --root-user-action=ignore --no-deps ${PYTHON_PACKAGES} \
           && ${CONVERT}"

printf '\nBanner ready: resources/banner/3dsFM.cgfx and resources/banner/jingle.wav\n'
if [[ -n "${STILL}" ]]; then
    mv "${WORK}/3dsFM-still.cgfx" "${PROJECT_ROOT}/build/3dsFM-still.cgfx"
    printf 'Still banner: build/3dsFM-still.cgfx\n'
fi
