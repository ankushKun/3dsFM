# 3dsFM

**Your music, in stereo, on the Nintendo 3DS.**

A music player for the 3DS that streams from a small self-hosted backend, keeps songs on the SD card for offline listening, and plays on with the lid closed. Built on the same toolchain as [3dsDL](https://github.com/ankushKun/3dsDL) with a Citro2D interface designed to feel like it shipped with the console.

Website: https://ankush.one/3dsFM/

> **Status:** plays music from the SD card. Put albums under `sdmc:/Music` and 3dsFM finds them, reads their tags and cover art, and plays MP3, Ogg Vorbis, Opus and WAV with the lid open or closed. Streaming from a server, saving songs and pairing have their screens in place but are switched off until the server exists. See [docs/DESIGN.md](docs/DESIGN.md) for the design system and [docs/PLAN.md](docs/PLAN.md) for what comes next.

## Using it

Copy music to the SD card as folders, one album per folder, with the artist as the folder above it if you like:

```text
sdmc:/Music/Miki Matsubara/Pocket Park/01 - Mayonaka no Door.mp3
sdmc:/Music/Miki Matsubara/Pocket Park/cover.jpg
```

Tags are read from ID3v1 and ID3v2 (MP3), Vorbis comments (Ogg, Opus) and the INFO chunk (WAV). Untagged files take their title from the file name and their album and artist from the folders. Cover art comes from `cover`, `folder`, `front` or `album` image files in the folder, or from the first track with embedded art.

| Input | Action |
| --- | --- |
| Circle Pad / D-pad | Move focus |
| A | Open, play, pause |
| B | Back |
| X | Save to SD card (needs a server, shows a notice) |
| Y | Menu for the focused song or album |
| L / R | Previous and next song, from any screen |
| START | Play or pause from anywhere; with SELECT held, quit |
| SELECT | Visualizer |
| Touch | Everything above, on the bottom screen |

Sections on the bottom screen: Home (albums), Songs, Artists, Up next, Downloads. Round buttons: Search, Now Playing, Settings.

## Layout

```text
source/app/      console-only code: screens (app.cpp), UI primitives (ui.cpp), art
                 textures (art.cpp), decoders and the NDSP player, platform services
source/core/     portable logic with no 3DS headers: tag reader, library scanner,
                 play queue, time formatting
tests/           host-side unit tests for source/core (ASan + UBSan)
scripts/         container build, emulator install and launch, fonts, test music
resources/       launcher icon, HOME Menu banner and jingle, CIA rsf, font sources
romfs/fonts/     M PLUS Rounded 1c as BCFNT (SIL OFL), see romfs/fonts/LICENSE.md
docs/            design system, screen plan, roadmap
site/            the website, published by GitHub Pages (see site/README.md)
```

Anything that can run without a console lives in `source/core` and has a test. Anything that touches libctru lives in `source/app`.

## Develop

Requirements on macOS: Docker (Docker Desktop, or the CLI with Colima), Python 3, and a C++17 compiler for the host tests. No native devkitPro install is needed.

```sh
make -C tests check                 # host unit tests, under sanitizers
THREEDSFM_3DSX_ONLY=1 ./scripts/build.sh   # fast 3DSX build into dist/
./scripts/build.sh                  # full build: 3DSX, SMDH, CIA, checksums
./scripts/install-emulator.sh       # one-time: pinned Azahar into ~/Applications
./scripts/run.sh                    # build if needed and open in Azahar
./scripts/run.sh --build            # rebuild, then open
./scripts/make-test-music.sh "$HOME/Library/Application Support/Azahar/sdmc/Music"
                                    # a fake library of tones in every format (needs ffmpeg)
./scripts/make-fonts.sh             # regenerate romfs/fonts from the TTFs
```

For crash testing in the emulator, a file at `sdmc:/3ds/3dsFM/smoke.txt` with space-separated key names (`A B X Y L R UP DOWN LEFT RIGHT START SELECT WAIT`) is played back one key every 20 frames, and each step is logged to `sdmc:/3ds/3dsFM/log.txt`. Frame timings are logged a few times at ten-second intervals. Empty files named `debug-noart`, `debug-notext`, `debug-nodots`, `debug-sysfont` or `debug-c2dcircles` in the same folder switch those parts off for A/B testing.

Two emulator notes learned the hard way: Azahar's screenshot action produces a black image for this homebrew, and typing into its window can trip hotkeys (a `-` lowers the speed limit by 5%). If the status bar shows a speed below 100%, reset `frame_limit` in its config.

### Threads

The UI runs on the main thread. Audio decoding runs on the second core when the system allows it. Album art decoding and the library scan each run on a low-priority thread on the first core, so they only use the time the UI spends waiting for vsync and never stall a frame.

The build runs inside the digest-pinned `devkitpro/devkitarm` image, so two builds of the same commit give the same `3dsFM.3dsx`.

Releases: bump `VERSION`, commit, then push a matching tag (`git tag v0.1.0 && git push origin v0.1.0`). GitHub Actions builds the 3DSX and CIA, checks the checksums and publishes the release, which the website's QR codes point at through `releases/latest/download/`.

### Audio in the emulator and on a console

NDSP needs the console's DSP firmware. Dump it once with [DSP1](https://github.com/zoogie/DSP1) and copy `dspfirm.cdc` to:

| Where | Path |
| --- | --- |
| Console SD card | `sdmc:/3ds/dspfirm.cdc` |
| Azahar | `~/Library/Application Support/Azahar/sdmc/3ds/dspfirm.cdc` |

Without it the app runs silently and says so when you try to play. In Azahar any file of a few kilobytes at that path works, because its DSP is emulated at a high level.

The emulator also lacks the console's shared font and substitutes a wide one, which is why 3dsFM ships its own fonts in romfs.

## Install on a console

| Build | Path |
| --- | --- |
| Homebrew Launcher | `sdmc:/3ds/3dsFM/3dsFM.3dsx` |
| HOME Menu | Install `dist/3dsFM.cia` with FBI. Title ID `0004000003D5F100`. |

## License

GPLv3, like 3dsDL.
