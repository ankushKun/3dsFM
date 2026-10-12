#!/usr/bin/env bash
# Builds a test library from a music.json playlist manifest (the format the
# ankush.one site uses: tracks with videoId, title, artist, durationSec and a
# thumb path). Audio is fetched with yt-dlp as MP3 and tagged with the
# manifest's title and artist, a compilation album name, the track number and
# the cover from the manifest's covers folder.
#
#   ./scripts/import-playlist.sh <music.json> <covers dir> <output Music dir> [limit]
#
# Needs yt-dlp, ffmpeg and python3. Only use this with playlists you have the
# right to download.
set -Eeuo pipefail
MANIFEST="${1:?music.json}"
COVERS="${2:?covers dir}"
OUT="${3:?output dir}"
LIMIT="${4:-0}"
for tool in yt-dlp ffmpeg python3; do command -v "$tool" >/dev/null || { echo "$tool is required" >&2; exit 1; }; done

ALBUM="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1])).get("title","Playlist"))' "$MANIFEST")"
DEST="${OUT}/Various Artists/${ALBUM}"
mkdir -p "${DEST}"
WORK="$(mktemp -d)"; trap 'rm -rf "${WORK}"' EXIT

python3 - "$MANIFEST" "$LIMIT" <<'PY' > "${WORK}/tracks.tsv"
import json, sys
m = json.load(open(sys.argv[1]))
limit = int(sys.argv[2])
for i, t in enumerate(m["tracks"], 1):
    if limit and i > limit: break
    print("\t".join([str(i), t["videoId"], t["title"], t["artist"], str(t.get("durationSec", 0)), t.get("thumb", "")]))
PY

n=0
while IFS=$'\t' read -r index videoId title artist duration thumb; do
    safe="$(printf '%s' "${title}" | tr '/:*?"<>|' '_')"
    file="${DEST}/$(printf '%02d' "${index}") - ${safe}.mp3"
    if [[ -s "${file}" ]]; then
        echo "kept    ${file##*/}"
        continue
    fi
    cover="${COVERS}/${videoId}.jpg"
    [[ -f "${cover}" ]] || cover="${COVERS}/$(basename "${thumb}")"
    echo "fetch   ${index}. ${artist} - ${title}"
    if ! yt-dlp --quiet --no-warnings -f 'bestaudio/best' --no-playlist --extractor-args 'youtube:player_client=android' \
            -o "${WORK}/${videoId}.%(ext)s" "https://www.youtube.com/watch?v=${videoId}" </dev/null; then
        echo "skip    ${title} (download failed)" >&2
        continue
    fi
    src="$(ls "${WORK}/${videoId}".* | head -n 1)"
    if [[ -f "${cover}" ]]; then
        ffmpeg -nostdin -loglevel error -y -i "${src}" -i "${cover}" -map 0:a -map 1:v -c:a libmp3lame -b:a 160k -c:v mjpeg \
            -id3v2_version 3 -metadata title="${title}" -metadata artist="${artist}" -metadata album="${ALBUM}" \
            -metadata album_artist="Various Artists" -metadata track="${index}" -metadata:s:v title="Album cover" \
            -metadata:s:v comment="Cover (front)" "${file}"
    else
        ffmpeg -nostdin -loglevel error -y -i "${src}" -map 0:a -c:a libmp3lame -b:a 160k -id3v2_version 3 \
            -metadata title="${title}" -metadata artist="${artist}" -metadata album="${ALBUM}" \
            -metadata album_artist="Various Artists" -metadata track="${index}" "${file}"
    fi
    rm -f "${src}"
    n=$((n + 1))
done < "${WORK}/tracks.tsv"
echo "Imported ${n} new track(s) into ${DEST}"
ls "${DEST}"
