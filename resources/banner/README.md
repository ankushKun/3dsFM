# HOME Menu banner and icon

When 3dsFM is selected on the HOME Menu, the top screen shows its banner,
with a short jingle. This folder holds an animated 3D banner model, like the
ones official games have, and everything it is made from, as code. The
pipeline and its converter come from 3dsDL, whose banner went through five
test builds on a console before it worked; the rules it learned are kept
here unchanged.

> **Status: built, not yet confirmed on a console.** Check it on hardware
> before a release that packs it (see Testing). `scripts/build.sh` packs the
> flat picture instead when the CGFX is missing or `THREEDSFM_FLAT_BANNER=1`.

The scene is a glossy dark record with a Clearwater-blue label. It sways and
spins while a tonearm swings in from the right and drops its needle at 0.7 s;
the record squashes happily, three music notes pop out from under the needle
and float off, and the 3dsFM wordmark floats in front, gently bobbing. Two
small stars twinkle in the corners. The loop is four seconds long.

The 48 × 48 HOME Menu icon, the flat fallback banner, the website's icons and
the social image are rendered from the same model, so they always match.

| File | What it is |
| --- | --- |
| `model.py` | Builds the scene as code: rounded, bevelled shapes, materials and rigid keyframe animation, written out as `banner.glb`. |
| `logo.html` | The wordmark texture, rendered to `logo.png` (256 × 128). |
| `jingle.py` | Synthesises `jingle.wav`, an original tune timed to the animation. |
| `build-cgfx.py` | Converts `banner.glb` to `3dsFM.cgfx` with [pycgfx](https://github.com/skyfloogle/pycgfx), names every mesh's node, rebuilds the bone lookup tree, makes the wordmark a billboard and sets the lighting. |
| `render.html`, `render.sh` | Render the model in headless Chrome with three.js, framed by the HOME Menu's banner camera, to make the icon and the other images. |
| `banner.glb`, `3dsFM.cgfx`, `jingle.wav`, `logo.png` | The generated files, committed so that `scripts/build.sh` needs none of these tools. |

## Rebuilding

```sh
resources/banner/render.sh   # logo texture, model, icon and images (needs Chrome)
scripts/make-banner.sh       # jingle, model and CGFX (needs Docker)
```

## Rules the model follows

Breaking these can freeze the HOME Menu on a real console even when an
emulator shows the banner fine. They are explained in full in 3dsDL's
banner README; in short:

- Rigid animation only: every moving part is its own node with translation,
  rotation and scale keys. No skinning.
- The model is called `COMMON`, every mesh names its node, and every name
  lookup in every table resolves; `build-cgfx.py` checks this and refuses
  to write a CGFX otherwise.
- The wordmark is a Y-axis billboard with a hard-edged cut-out (alpha MASK),
  never blended transparency.
- Nothing scales to nearly nothing: notes hide behind the record instead.
- About 2,000 triangles at most, textures at most 256 px on a side, the
  CGFX under 512 KiB, and the sound stereo and under 3 seconds.
- The camera is fixed at (0, 1, 44.786) looking down -z with a 30° field of
  view, so the plane z = 0 shows x from -20 to 20 and y from -11 to 13.

## Testing

Install a CIA built with the banner over FBI and select 3dsFM on the HOME
Menu. If the HOME Menu crashes or freezes, install a flat banner build over
it (`THREEDSFM_FLAT_BANNER=1 ./scripts/build.sh`) and look at what changed in
the model since the last good one.
