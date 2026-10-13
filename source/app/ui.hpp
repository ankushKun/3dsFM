#pragma once

#include <3ds.h>
#include <citro2d.h>

#include <string>

// Clearwater drawing primitives for Citro2D. All sizes are console pixels.
namespace ui {

// Theme colours (see docs/DESIGN.md). They are set by setTheme(), dark by
// default, so every drawing call reads the current theme.
extern u32 TopBg, BottomBg, Sheet, Ink, InkSoft, InkFaint, Meta, Line, Blue, BlueDeep, BlueLight, BlueTint, BlueText,
    DarkPill, PillText, ButtonDark, ButtonDarkIcon, KeyText, ShadowBase, Tag, TagText, Danger, White, Dim;

void setTheme(bool dark);
bool darkTheme();

u32 withAlpha(u32 color, u8 alpha);
u32 mix(u32 a, u32 b, float t);

void init();
void shutdown();
void beginFrame();
// Tell the drawing code which screen is current, for clipping.
void beginScreen(float width, float height);

enum class Align { Left, Center, Right };
enum class Weight { Regular, Bold };

// Text at `size` pixels tall. maxWidth > 0 trims with an ellipsis.
void text(const std::string &s, float x, float y, float size, u32 color, Align align = Align::Left, float maxWidth = 0.0f, Weight weight = Weight::Regular);
float textWidth(const std::string &s, float size, Weight weight = Weight::Regular);
// Shrinks the size until the text fits maxWidth, down to minSize.
float fitSize(const std::string &s, float size, float minSize, float maxWidth, Weight weight = Weight::Bold);
// Text that scrolls sideways when it is wider than maxWidth. `slot` names the
// place on screen so each keeps its own scroll phase; the phase resets when
// the text changes.
void marquee(const char *slot, const std::string &s, float x, float y, float size, u32 color, float maxWidth, Weight weight = Weight::Regular);
void marqueeTick();
// Clip subsequent drawing to a rectangle on the current screen.
void clipBegin(float x, float y, float w, float h);
void clipEnd();

void rect(float x, float y, float w, float h, u32 color);
void roundRect(float x, float y, float w, float h, float r, u32 color);
// Paints the four corners of a rectangle in the surrounding colours so a
// square image underneath reads as rounded. top and bottom colours allow a
// vertical gradient behind it.
void roundCorners(float x, float y, float w, float h, float r, u32 top, u32 bottom);
void circle(float cx, float cy, float r, u32 color);
void ring(float cx, float cy, float r, float thickness, u32 color, u32 inside);
void line(float x0, float y0, float x1, float y1, float thickness, u32 color);
void gradientV(float x, float y, float w, float h, u32 top, u32 bottom);
void roundRectGradient(float x, float y, float w, float h, float r, u32 top, u32 bottom);
void gradientH(float x, float y, float w, float h, u32 left, u32 right);
void dots(float x, float y, float w, float h);

enum class Icon {
    Home, Note, Grid, List, Search, Queue, Gear, Play, Pause, Prev, Next, Shuffle, Repeat, RepeatOne,
    Headphones, Wifi, Battery, Clock, ChevronRight, ChevronLeft, Close, Plus, Minus, Trash, Grip,
    Keyboard, Bars, Person, Check, Retry, Lyrics, Back, Album
};
// Draws an icon centred on cx, cy inside a box of `size`. `bg` is used for
// hollow shapes (rings) so they cut cleanly on their background.
void icon(Icon which, float cx, float cy, float size, u32 color, u32 bg = Sheet);

// A legend key: a small grey circle with a letter.
void keyCap(const char *letter, float cx, float cy, u32 color = Meta);
// Right-aligned legend of (key, label) pairs ending at x.
struct LegendItem {
    const char *key;
    const char *label;
};
void legend(const LegendItem *items, int count, float rightX, float y);

// focused draws the cursor ring; iconColor (0 for the default) marks a
// button whose view is the current one without a second ring.
void roundButton(float cx, float cy, float r, Icon which, bool dark, bool focused = false, u32 iconColor = 0);
void pill(const std::string &label, float x, float y, u32 bg, u32 fg, float size = 7.0f);
void tag(const std::string &label, float x, float y, u32 outline, u32 fg);
void progressBar(float x, float y, float w, float h, float fraction, u32 track = Line);
void equaliser(float x, float y, float level, u32 color, int frame);
void toggle(float x, float y, bool on);

bool hit(float x, float y, float w, float h, float px, float py);

} // namespace ui
