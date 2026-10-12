#!/usr/bin/env bash
# Builds romfs/fonts/*.bcfnt from M PLUS Rounded 1c (SIL OFL 1.1) with
# devkitPro's mkbcfnt inside the pinned toolchain image. The glyph whitelist
# keeps the files small: Latin, kana and punctuation.
set -Eeuo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="${ROOT}/resources/fonts/src"
IMAGE="${THREEDSFM_BUILD_IMAGE:-devkitpro/devkitarm:20260610@sha256:116afba8df8453961de2936ffab20dd441edf4d682856c1ec8b0e53d7ed0bbf5}"
mkdir -p "${SRC}" "${ROOT}/romfs/fonts"
for weight in Medium Bold; do
    [[ -f "${SRC}/MPLUSRounded1c-${weight}.ttf" ]] || curl -fsSL -o "${SRC}/MPLUSRounded1c-${weight}.ttf" \
        "https://github.com/google/fonts/raw/main/ofl/mplusrounded1c/MPLUSRounded1c-${weight}.ttf"
done
python3 - "${SRC}/whitelist.txt" <<'PY'
import sys
ranges=[(0x20,0x7E),(0xA0,0x17F),(0x2010,0x2027),(0x2030,0x203A),(0x20AC,0x20AC),(0x2190,0x2193),(0x2600,0x2606),(0x266A,0x266C),(0x3000,0x30FF),(0xFF01,0xFF5E)]
cps=[hex(c) for a,b in ranges for c in range(a,b+1)]
open(sys.argv[1],'w').write(' '.join(cps))
PY
docker run --rm --user "$(id -u):$(id -g)" --volume "${ROOT}:/project" --workdir /project/resources/fonts/src "${IMAGE}" \
    bash -lc 'for s in 24 12; do mkbcfnt -s $s -w whitelist.txt -o ../../../romfs/fonts/medium$s.bcfnt MPLUSRounded1c-Medium.ttf && mkbcfnt -s $s -w whitelist.txt -o ../../../romfs/fonts/bold$s.bcfnt MPLUSRounded1c-Bold.ttf; done'
ls -la "${ROOT}/romfs/fonts"
