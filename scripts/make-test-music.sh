#!/usr/bin/env bash
# Generates a small fake music library for testing the scanner and the
# decoders: three albums of short tones in MP3 (with ID3v2 tags and embedded
# cover art), Ogg Vorbis, Opus and WAV. Needs ffmpeg.
#
#   ./scripts/make-test-music.sh <output dir>      e.g. the emulator's sdmc/Music

set -Eeuo pipefail
OUT="${1:?output directory}"
command -v ffmpeg >/dev/null || { echo "ffmpeg is required" >&2; exit 1; }
WORK="$(mktemp -d)"; trap 'rm -rf "${WORK}"' EXIT

cover() { # cover <path> <color1> <color2>
    ffmpeg -loglevel error -y -f lavfi -i "color=c=$2:s=300x300" -f lavfi -i "color=c=$3:s=160x160" \
        -filter_complex "[0][1]overlay=70:70" -frames:v 1 "$1"
}

tone() { # tone <out> <freq> <seconds> <codec args...> 
    local out="$1" freq="$2" secs="$3"; shift 3
    ffmpeg -loglevel error -y -f lavfi -i "sine=frequency=${freq}:duration=${secs}:sample_rate=44100" \
        -af "volume=0.3,aformat=channel_layouts=stereo" "$@" "${out}"
}

album() { # album <artist> <album> <year> <ext> <cover1> <cover2> titles...
    local artist="$1" album="$2" year="$3" ext="$4" c1="$5" c2="$6"; shift 6
    local dir="${OUT}/${artist}/${album}"; mkdir -p "${dir}"
    cover "${WORK}/cover.png" "${c1}" "${c2}"
    local n=1
    for title in "$@"; do
        local freq=$((220 + n * 110)) secs=$((6 + n * 2)) file="${dir}/$(printf '%02d' "$n") - ${title}.${ext}"
        case "${ext}" in
            mp3)
                tone "${WORK}/t.wav" "${freq}" "${secs}" -c:a pcm_s16le
                ffmpeg -loglevel error -y -i "${WORK}/t.wav" -i "${WORK}/cover.png" -map 0:a -map 1:v \
                    -c:a libmp3lame -b:a 128k -c:v png -id3v2_version 3 \
                    -metadata title="${title}" -metadata artist="${artist}" -metadata album="${album}" \
                    -metadata date="${year}" -metadata track="${n}" \
                    -metadata:s:v title="Album cover" -metadata:s:v comment="Cover (front)" "${file}" ;;
            ogg)
                tone "${file}" "${freq}" "${secs}" -c:a vorbis -strict -2 \
                    -metadata title="${title}" -metadata artist="${artist}" -metadata album="${album}" \
                    -metadata date="${year}" -metadata track="${n}"
                cp "${WORK}/cover.png" "${dir}/cover.png" ;;
            opus)
                tone "${file}" "${freq}" "${secs}" -c:a opus -strict -2 -b:a 96k \
                    -metadata title="${title}" -metadata artist="${artist}" -metadata album="${album}" \
                    -metadata date="${year}" -metadata track="${n}"
                cp "${WORK}/cover.png" "${dir}/folder.jpg.png" 2>/dev/null || true
                ffmpeg -loglevel error -y -i "${WORK}/cover.png" "${dir}/folder.jpg" ;;
            wav)
                tone "${file}" "${freq}" "${secs}" -c:a pcm_s16le ;;
        esac
        n=$((n + 1))
    done
}

album "Miki Matsubara" "Pocket Park" 1980 mp3 "#F6D96B" "#F06A4F" "Mayonaka no Door" "Niji Iro no Ame" "Daydream"
album "Tomoko Aran" "Fuyu-Kukan" 1983 ogg "#2FA37A" "#1F1F1F" "Midnight Pretenders" "I'm in Love" "Slow Nights"
album "Yasuha" "Flyday Chinatown" 1981 opus "#F2A1B8" "#FFFFFF" "Fly-Day Chinatown" "Tokyo Night" "Bye Bye"
mkdir -p "${OUT}/Loose files"
tone "${OUT}/Loose files/untagged tone.wav" 440 5 -c:a pcm_s16le
rm -f "${OUT}"/*/*/folder.jpg.png
echo "Wrote a test library to ${OUT}"
find "${OUT}" -type f | sort
