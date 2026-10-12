#!/usr/bin/env bash
# Builds the 3DSX if needed and opens it in Azahar.
#
#   ./scripts/run.sh            launch dist/3dsFM.3dsx (building when missing)
#   ./scripts/run.sh --build    rebuild first
#
# Audio in the emulator needs the DSP firmware dump at
# <Azahar user dir>/sdmc/3ds/dspfirm.cdc, the same file a console needs.
# Without it the app still runs and shows a notice on the bottom screen.

set -Eeuo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
TARGET="${PROJECT_ROOT}/dist/3dsFM.3dsx"

for candidate in "${AZAHAR_APP:-}" "${HOME}/Applications/Azahar.app" "/Applications/Azahar.app"; do
    if [[ -n "${candidate}" && -d "${candidate}" ]]; then
        APP="${candidate}"
        break
    fi
done
if [[ -z "${APP:-}" ]]; then
    echo "Azahar is not installed. Run ./scripts/install-emulator.sh first." >&2
    exit 1
fi

if [[ "${1:-}" == "--build" || ! -f "${TARGET}" ]]; then
    THREEDSFM_3DSX_ONLY=1 "${SCRIPT_DIR}/build.sh"
fi

echo "Launching ${TARGET} in ${APP}"
open -na "${APP}" --args "${TARGET}"
