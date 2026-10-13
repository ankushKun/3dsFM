# Clearwater: the 3dsFM design language (second pass)

The design canvas with the mockups is at https://claude.ai/artifact/18w77Adnbrv8mf64wHGMbF (private until shared). This file is the version the code follows. All sizes are console pixels; the canvas is drawn at 2x.

The target is the Nintendo 3DS HOME Menu, the Wii U GamePad menu and the iiSU dual-screen concept art: white, art-led, small type, one blue. The first pass was built from web habits and read as generated. This one is built from the console.

## What the reference does

- **Top screen is a showcase.** Album art bleeds off the left edge and fogs into white. The title sits on it like a game logo. Everything else is corner metadata at 8 to 9 px: recent covers and a count top-left, headphones, Wi-Fi, battery and the clock top-right, a small Playing thumbnail bottom-left, a stat pill and the button legend bottom-right.
- **Bottom screen is a tool.** A row of five small monochrome section icons on the left, three round buttons on the right, a four-wide grid of square art tiles below, nothing along the bottom edge. The active section icon takes the blue.
- **Selection stays put.** The concept art grows the focused row into a blue block; on the console that made tiles vanish with every d-pad press, so 3dsFM keeps the grid stable. The focused tile gets a blue ring, and a fixed blue strip under the grid shows the title, one line of metadata, and the play and menu buttons. Two rows of tiles are visible and scroll as focus moves.
- **Menus are sheets.** A small white panel floats over a dimmed screen, with a column of round buttons beside it. Detail views are one white card per screen, never a stack.
- **Status is a dark pill on the tile.** Playing, Saving 64%, Waiting, Retry, New.

## The slop audit

Sources: [Impeccable's catalog of AI design tells](https://www.impeccable.style/slop), [a UX guide on detecting AI slop](https://github.com/Laith0003/ux-skill/wiki/How-to-detect-AI-slop-in-your-design), [saasui on SaaS that looks generated](https://www.saasui.design/blog/saas-ui-looks-ai-generated), [the purple gradient essay](https://prg.sh/ramblings/Why-Your-AI-Keeps-Building-the-Same-Purple-Gradient-Website).

What the first pass did, and the rule that replaces it:

| Tell | First pass | Rule now |
| --- | --- | --- |
| Cards inside cards, hairline plus wide shadow | Every element in a shadowed white panel | Only sheets, menus and dialogs cast a shadow. Tiles get a 1 px darkening at the bottom edge. |
| Extreme radii, pill buttons | Pill buttons everywhere, 999 px radii | Round icon buttons 18 px, sheets radius 8, tiles radius 10, text buttons radius 5. |
| Purple and gradient washes | Orange-to-purple art, blue gloss on everything | No purple. The only gradient is the HOME Menu blue on the selection block and the progress bar. |
| Dark mode with glowing blobs | Night backdrop with blurred colored halos | Now Playing is white with fogged art, like every other screen. The visualizer draws in the art's own two colors on white. |
| Glossy highlights | Inset white highlight on every button | None. Flat fills. |
| Uppercase micro badges, label above heading | SAVED, CLOUD, "ALBUM" above the title | Sentence case tags, outlined not filled. The title is the first thing, with the type below it. |
| Explanatory copy on screens | "Press X on any song to…" on most screens | The legend in the corner is the only help text. |
| Icon over heading hero, giant icons | The pairing screen | Logo text left, a fan of sleeves right. Icons are 11 px glyphs. |
| Flat type hierarchy, many sizes | Seven sizes between 9 and 22 | Three sizes plus the showcase title. |
| Monotonous spacing, identical card grids | Equal gaps, every row a card | Rows are 20 px tall with 3 px gaps, tiles 64 px with 7 px gaps, chrome hugs the corners. |
| Placeholder illustrations | Gradient squares as album art | Stand-in sleeves drawn as record covers, replaced by real art from the server. |

## Tokens

### Color

Dark mode is the default; the light palette is the HOME Menu look and stays available under Settings, Look. Both share the selection blue so the signature survives, and the art fog blends into whichever background is current.

| Role | Dark | Light |
| --- | --- | --- |
| Top screen | `#15171B` | `#FBFBFC` |
| Bottom screen, dot grid | `#1C1F25`, dots `#343942` | `#EDF0F4`, dots `#D6DBE3` |
| Sheet | `#262A31` | `#FFFFFF` |
| Ink, soft, faint | `#ECEFF3`, `#C5CBD3`, `#6B7482` | `#2A2E35`, `#3D434C`, `#B5BCC6` |
| Meta, line | `#9AA3AF`, `#3A3F48` | `#7A8290`, `#D6DBE3` |
| Blue tint, blue text | `#1E3A55`, `#8FD1FF` | `#E4F2FF`, `#1C6FC2` |
| Status pill, dark button | light `#ECEFF3` with dark text | dark `#2A2E35` with white text |
| Tag text, danger | `#F5C542`, `#FF6B5B` | `#B07A0A`, `#C0392B` |
| Shadows | black at low alpha | `#1E2838` at low alpha |


| Token | Value | Use |
| --- | --- | --- |
| Top screen | `#FBFBFC` | Art fogs into it |
| Bottom screen | `#EDF0F4`, dot grid `#D6DBE3` every 7 px | HOME Menu texture |
| Sheet | `#FFFFFF`, shadow `rgba(30,40,60,.10)` 2 px down 7 px blur | Menus, cards, dialogs |
| Select | `#8FD1FF` to `#3B97EA` vertical | Selection block, focused row, progress fill, active section icon (`#2F8FE8`) |
| Ink | `#2A2E35` | Text, icons, the dark play button |
| Meta | `#7A8290` | Metadata, inactive icons, legend |
| Line | `#D6DBE3` | Progress track, dividers |
| Dark pill | `rgba(42,46,53,.92)` | Status on a tile |
| Tag | outlined 1 px, `#F0B429` on `#B07A0A` text for Saved, blue for Liked, grey for Cloud | Never filled |
| Danger | `#C0392B` | Remove, retry pill, Wi-Fi lost |

### Type

M PLUS Rounded 1c on the canvas, the system font on the console.

| Role | Console px / weight | Use |
| --- | --- | --- |
| Showcase | 30 / 700 | The title on the top screen |
| Title | 12 to 15 / 700 | Row text, card titles, the selection block |
| Body | 11 to 13 / 500 | Artist, descriptions, settings |
| Meta | 9 to 10 / 500 | Corner metadata, durations, legend, pills |

The console build ships M PLUS Rounded 1c Medium and Bold as BCFNT files (`romfs/fonts`), so the type matches the canvas on hardware and in the emulator. The first on-device pass used 8 px metadata and 11 px icons and was unreadable; everything was scaled up by about a third, and tab icons are 15 px.

Sentence case everywhere. No uppercase, no tracking.

### Shape and space

Tiles 64 px with 10 px corners, 7 px apart, four across starting 18 px in and 33 px down. Round buttons 20 px, the main play button 32 px. Sheets radius 8 with 4 px inner padding and 20 px rows. Legend keys are 11 px grey circles with the letter in white. Circles are drawn as triangle fans because the emulator's renderer skips Citro2D's circle primitive.

### Motion and sound

Focus moves in 100 ms, no overshoot. A tile grows into the block in 160 ms. Sheets fade and rise 3 px in 140 ms. Art crossfades in 300 ms. Tick on focus, pop on select, off by default.

### Stereo depth

Build note, not a visual: on the top screen the fogged art is drawn 3 px behind the surface and the showcase title 2 px in front, scaled by the slider. Nothing else moves.

## What is live

Library from the SD card, albums, songs, artists, search, up next, now playing, visualizer, settings, lid-closed playback. The pairing screen, stream quality, saving to the SD card, liking and the Downloads section were removed from the app in 0.5.0; their mockups stay on the canvas as history.

## Input map

| Input | Action |
| --- | --- |
| Circle Pad / D-pad | Move focus |
| A | Select, play, pause |
| B | Back |
| X | Save to SD card |
| Y | Open the sheet menu |
| L / R | Previous and next song, from any screen |
| START | Play or pause from anywhere |
| SELECT | Visualizer |
| Touch | Everything above, on the bottom screen only |

## Screens

| Id | Top screen | Bottom screen |
| --- | --- | --- |
| S01 | Wordmark as a logo, one line, a fan of sleeves fogging in from the right | Camera viewfinder with corner brackets, one card to type the address |
| S02 | Showcase of the focused album | Section icons, round buttons, 4x3 tile grid, Playing and New pills |
| S03 | Showcase with tags | Focused tile grown into the blue block with play and save buttons, neighbours faded |
| S04 | Showcase with Saved tags | Album header card with shuffle, save and play, then numbered rows |
| S05 | Showcase of the playing track, thin progress line, Up next thumbnail bottom-left | Mini player card, round transport with a dark pause button, up next rows |
| S06 | Lyric lines over fogged art, the current line large | Mini player, nudge buttons, timed lyric rows |
| S07 | Showcase of the next track | Queue rows with drag grips, the current row with the equaliser glyph |
| S08 | Bars in the art's two colors on white, title in the corner | Dimmed, style picker sheet, mini player, one pause button |
| S09 | Showcase of the focused result with its albums | Search field, mixed results with the type at the right |
| S10 | One card: current download, SD card usage | Tile grid with Saving, Retry and Waiting pills |
| S11 | One card explaining the focused setting | One sheet with grouped settings, switches and values |
| S12 | Wi-Fi dialog card over fogged art, toast at the bottom | Sheet menu, round button column, paused mini player, dimmed grid |
