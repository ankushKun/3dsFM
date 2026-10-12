# Build plan

## Architecture

```text
3DS (3dsFM)                        Backend (self-hosted, one process)
───────────────────────            ──────────────────────────────────
ui/        screens, components     /search?q=      ytmusicapi -> compact JSON
audio/     Opus decode, NDSP       /stream/{id}    yt-dlp -> ffmpeg -> Opus 96k 48 kHz, HTTP ranges
library/   SQLite-free flat index  /art/{id}       256 and 32 px art, dominant color pair
net/       libcurl client          /lyrics/{id}    LRCLIB synced lyrics
core/      queue, formatting       /library        playlists, likes, history for the paired consoles
```

The console never talks to YouTube. It only ever sees one audio format, so the decoder path stays small and the DSP keeps up on an Old 3DS. Pairing is a QR code carrying the server URL and a token. Keep the backend personal; do not run a public instance.

## Order

1. **Scaffold** (done): container build, host tests, Azahar launcher.
2. **Local playback** (done): SD card library with tags and art, MP3, Ogg Vorbis, Opus and WAV decoders feeding NDSP on a thread, lid-closed playback.
3. **Clearwater UI** (done): all screens in Citro2D with bundled fonts; server-only features drawn but disabled.
4. **Backend v1**: search and stream, a throwaway web page to prove transcoding and range requests.
5. **Pairing and streaming**: QR pairing, Opus streaming into the existing player, the server-backed Library sections.
6. **Downloads**: reuse 3dsDL's download engine and resume logic for saving tracks and art to the SD card.
7. **Lyrics** from the server, with `.lrc` sidecar files for local music.
8. **Stereo depth** on the top screen, animation polish, menu sounds, HOME Menu banner with 3D art, release.

## Decisions already made

- Opus over MP3 for streams: better at 96 kbps, lighter decoder. MP3 stays supported for local files.
- Flat files on the SD card for the library index, no SQLite.
- One accent for selection (Wave), one for playing (Sunny). Nothing else gets color.
- The top screen never has controls.
