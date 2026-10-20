# Website

The static site published at https://ankush.one/3dsFM/ by
`.github/workflows/pages.yml` whenever `site/` changes on `main`.

- `index.html`, `styles.css`, `app.js`: the page, themed after the DS and 3DS
  system software and sharing its bones with the 3dsDL site. It works
  without JavaScript. `app.js` adds the HOME Menu features, the status bar
  clock, the Notifications read state, the installer tabs, the device
  screens and their boot animation, the synthesised menu sounds and the
  scroll reveals, and shows the latest release version and file sizes from
  the GitHub API.
- `qr/`: install QR codes. They point at `releases/latest/download/...`, so
  they stay valid for every release. Regenerate with
  `scripts/make-site-qr.sh` only if the repository or asset names change.
- `fonts/`: M PLUS Rounded 1c and M PLUS 1 Code (text), DotGothic16 (the
  pixel face for system labels) and Gaegu (the handwriting on the doodles),
  Latin subsets, all under the SIL Open Font License (the `OFL-*.txt` files).
- `assets/`: the icons (a dark tile with a blue note, drawn in Python when
  the site was set up) and the social image (`banner.jpg`), plus the two
  console images below.

## Console images

| File | Used in | Source | Author | Licence |
| --- | --- | --- | --- | --- |
| `assets/new-3ds-angled.png` | hero | [File:New Nintendo 3DS (weiß) 20250129 HOF2650 RAW-Export.png](https://commons.wikimedia.org/wiki/File:New_Nintendo_3DS_(wei%C3%9F)_20250129_HOF2650_RAW-Export.png), resized to 1800 px and cropped to the console (1356 × 1177) | PantheraLeo1359531 | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) |
| `assets/new-3ds-front.png` | install | Nintendo's front-on New Nintendo 3DS product render, cut out at 736 × 736 with the screens blanked | Nintendo | © Nintendo, not freely licensed |

The CC BY photograph requires the credit that the page footer carries; the
footer also notes that it was cropped and that its screens are replaced. The
install render is Nintendo's own marketing image, credited in the footer too;
swap it for a freely licensed front view if one turns up.

### Live screens on the images

Both images show 3dsFM on their screens. The screens are HTML drawn at the
3DS's native sizes (400 × 240 and 320 × 240 px) using the same coordinates
as `app.cpp` (Now Playing in the hero; the install section shows the
QR code and steps for whichever method is picked), then projected onto the photographed screens with a CSS
`matrix3d()`. Each image's matrices live on `.device-angled` and
`.device-front` in `styles.css` and come from `scripts/screen-map`; see its
README. If an image is replaced, its matrices have to be recomputed, and so do the
hero doodles (drawn in the angled photo's 1356 × 1177 pixel space) and the
install render's LED position (`.led`, in its 736 × 736 pixel space).

To preview locally, run `python3 -m http.server -d site` and open
http://localhost:8000.

## Analytics

`app.js` starts with the same PostHog setup as the rest of ankush.one: the same project, sent through
the first-party proxy at `p1.ankush.one`, with autocapture, heatmaps, Web Vitals, error capture and
session replay (console and network calls included). Every event carries `site: "3dsFM"`, and since the
pages share the ankush.one origin, a visitor keeps one id and one replay session across this site, the
portfolio and the other project pages. Nothing loads on localhost or when the browser sends Global
Privacy Control.

Custom events go through `window.track(name, props)`: `install_method`, `feature_viewed`,
`faq_opened`, `menu_sounds`, `download_link_copied` and `buddy_opened` (the 3dsDL card). What the
site sends is listed on https://ankush.one/privacy/, so update that page when adding an event.
