#!/usr/bin/env bash
# Renders every image made from the banner model, so the icon, the banner,
# the README art and the website all come from one source:
#   logo.html          -> logo.png (the wordmark texture on the model)
#   model.py           -> banner.glb
#   render.html?icon   -> resources/icon-3dsfm.png (48 x 48, the HOME Menu icon)
#                         resources/icon-3dsfm-source.png (1024 x 1024)
#                         site/assets/icon-64.png, icon-256.png, apple-touch-icon.png
#   render.html?flat   -> resources/banner-3dsfm.png (the flat fallback banner)
#   render.html?promo  -> resources/banner-3dsfm-source.png, site/assets/banner.jpg
#
# Needs Google Chrome (headless) and network access for three.js. Run
# scripts/make-banner.sh afterwards to rebuild the CGFX from the new model.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${HERE}/../.." && pwd)"
CHROME="${CHROME:-/Applications/Google Chrome.app/Contents/MacOS/Google Chrome}"
OUT="$(mktemp -d)"
trap 'rm -rf "${OUT}"' EXIT

shoot() {
    "${CHROME}" --headless=new --use-angle=swiftshader --enable-unsafe-swiftshader \
        --hide-scrollbars --allow-file-access-from-files --virtual-time-budget=20000 "$@" 2>/dev/null
}

# Chrome writes RGB PNGs; the HOME Menu icon must carry an alpha channel.
rgba() {
    python3 -I - "$1" "$2" "${ROOT}/scripts/screen-map" <<'EOF'
import sys
sys.path.insert(0, sys.argv[3])
import pngio
w, h, ch, rows = pngio.decode(sys.argv[1])
out = []
for row in rows:
    if ch == 4:
        out.append(bytearray(row))
    else:
        px = bytearray()
        for x in range(w):
            px += bytes(row[x * ch:x * ch + 3]) + b"\xff"
        out.append(px)
pngio.encode(sys.argv[2], w, h, out)
EOF
}

printf '==> Wordmark texture\n'
shoot --default-background-color=00000000 --window-size=1024,512 --screenshot="${OUT}/logo.png" "file://${HERE}/logo.html"
sips -z 128 256 "${OUT}/logo.png" --out "${HERE}/logo.png" >/dev/null

printf '==> Model\n'
python3 "${HERE}/model.py" "${HERE}/logo.png" "${HERE}/banner.glb"

printf '==> Icon\n'
shoot --window-size=1024,1024 --screenshot="${OUT}/icon.png" "file://${HERE}/render.html?mode=icon&size=1024"
cp "${OUT}/icon.png" "${ROOT}/resources/icon-3dsfm-source.png"
sips -z 256 256 "${OUT}/icon.png" --out "${ROOT}/site/assets/icon-256.png" >/dev/null
sips -z 180 180 "${OUT}/icon.png" --out "${ROOT}/site/assets/apple-touch-icon.png" >/dev/null
sips -z 192 192 "${OUT}/icon.png" --out "${OUT}/icon-192.png" >/dev/null
sips -z 64 64 "${OUT}/icon-192.png" --out "${ROOT}/site/assets/icon-64.png" >/dev/null
sips -z 48 48 "${OUT}/icon-192.png" --out "${OUT}/icon-48.png" >/dev/null
rgba "${OUT}/icon-48.png" "${ROOT}/resources/icon-3dsfm.png"

printf '==> Flat banner\n'
shoot --window-size=512,256 --screenshot="${OUT}/flat.png" "file://${HERE}/render.html?mode=flat"
sips -z 128 256 "${OUT}/flat.png" --out "${ROOT}/resources/banner-3dsfm.png" >/dev/null

printf '==> Promo art\n'
shoot --window-size=1200,600 --screenshot="${OUT}/promo.png" "file://${HERE}/render.html?mode=promo&w=1200&h=600&logo=file://${OUT}/logo.png"
cp "${OUT}/promo.png" "${ROOT}/resources/banner-3dsfm-source.png"
sips -s format jpeg -s formatOptions 86 "${OUT}/promo.png" --out "${ROOT}/site/assets/banner.jpg" >/dev/null

printf '\nDone. Now run scripts/make-banner.sh to rebuild the CGFX.\n'
