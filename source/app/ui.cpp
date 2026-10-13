#include "ui.hpp"

#include "art.hpp"
#include "platform.hpp"

#include <cmath>
#include <cstring>
#include <map>
#include <vector>

namespace ui {

namespace {
C3D_Tex dotTex{};
Tex3DS_SubTexture dotSub{};
bool dotsReady = false;
}

u32 TopBg, BottomBg, Sheet, Ink, InkSoft, InkFaint, Meta, Line, Blue, BlueDeep, BlueLight, BlueTint, BlueText,
    DarkPill, PillText, ButtonDark, ButtonDarkIcon, KeyText, ShadowBase, Tag, TagText, Danger, White, Dim;

namespace {
bool themeDark = true;
void buildDots(u32 color);
}

namespace {
void buildDots(u32 color)
{
    if (dotsReady) {
        C3D_TexDelete(&dotTex);
        dotsReady = false;
    }
    // A repeating 8x8 tile with one soft dot, for the bottom screen.
    std::vector<uint8_t> rgba(8 * 8 * 4, 0);
    const uint8_t r = color & 0xFF;
    const uint8_t g = (color >> 8) & 0xFF;
    const uint8_t b = (color >> 16) & 0xFF;
    auto set = [&](int x, int y, uint8_t a) {
        uint8_t *p = rgba.data() + (y * 8 + x) * 4;
        p[0] = r; p[1] = g; p[2] = b; p[3] = a;
    };
    set(0, 0, 255);
    set(1, 0, 90);
    set(0, 1, 90);
    set(7, 0, 90);
    set(0, 7, 90);
    if (art::makeTexture(rgba.data(), 8, 8, dotTex, dotSub)) {
        C3D_TexSetWrap(&dotTex, GPU_REPEAT, GPU_REPEAT);
        C3D_TexSetFilter(&dotTex, GPU_NEAREST, GPU_NEAREST);
        dotsReady = true;
    }
}
} // namespace

bool darkTheme() { return themeDark; }

void setTheme(bool dark)
{
    themeDark = dark;
    White = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
    Blue = C2D_Color32(0x2F, 0x8F, 0xE8, 0xFF);
    BlueDeep = C2D_Color32(0x3B, 0x97, 0xEA, 0xFF);
    BlueLight = C2D_Color32(0x8F, 0xD1, 0xFF, 0xFF);
    Tag = C2D_Color32(0xF0, 0xB4, 0x29, 0xFF);
    if (dark) {
        TopBg = C2D_Color32(0x15, 0x17, 0x1B, 0xFF);
        BottomBg = C2D_Color32(0x1C, 0x1F, 0x25, 0xFF);
        Sheet = C2D_Color32(0x26, 0x2A, 0x31, 0xFF);
        Ink = C2D_Color32(0xEC, 0xEF, 0xF3, 0xFF);
        InkSoft = C2D_Color32(0xC5, 0xCB, 0xD3, 0xFF);
        InkFaint = C2D_Color32(0x6B, 0x74, 0x82, 0xFF);
        Meta = C2D_Color32(0x9A, 0xA3, 0xAF, 0xFF);
        Line = C2D_Color32(0x3A, 0x3F, 0x48, 0xFF);
        BlueTint = C2D_Color32(0x1E, 0x3A, 0x55, 0xFF);
        BlueText = C2D_Color32(0x8F, 0xD1, 0xFF, 0xFF);
        DarkPill = C2D_Color32(0xEC, 0xEF, 0xF3, 0xEB);
        PillText = C2D_Color32(0x15, 0x17, 0x1B, 0xFF);
        ButtonDark = C2D_Color32(0xEC, 0xEF, 0xF3, 0xFF);
        ButtonDarkIcon = C2D_Color32(0x15, 0x17, 0x1B, 0xFF);
        KeyText = C2D_Color32(0x15, 0x17, 0x1B, 0xFF);
        ShadowBase = C2D_Color32(0x00, 0x00, 0x00, 0xFF);
        TagText = C2D_Color32(0xF5, 0xC5, 0x42, 0xFF);
        Danger = C2D_Color32(0xFF, 0x6B, 0x5B, 0xFF);
        Dim = C2D_Color32(0x00, 0x00, 0x00, 0x99);
        buildDots(C2D_Color32(0x34, 0x39, 0x42, 0xFF));
    } else {
        TopBg = C2D_Color32(0xFB, 0xFB, 0xFC, 0xFF);
        BottomBg = C2D_Color32(0xED, 0xF0, 0xF4, 0xFF);
        Sheet = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
        Ink = C2D_Color32(0x2A, 0x2E, 0x35, 0xFF);
        InkSoft = C2D_Color32(0x3D, 0x43, 0x4C, 0xFF);
        InkFaint = C2D_Color32(0xB5, 0xBC, 0xC6, 0xFF);
        Meta = C2D_Color32(0x7A, 0x82, 0x90, 0xFF);
        Line = C2D_Color32(0xD6, 0xDB, 0xE3, 0xFF);
        BlueTint = C2D_Color32(0xE4, 0xF2, 0xFF, 0xFF);
        BlueText = C2D_Color32(0x1C, 0x6F, 0xC2, 0xFF);
        DarkPill = C2D_Color32(0x2A, 0x2E, 0x35, 0xEB);
        PillText = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
        ButtonDark = C2D_Color32(0x2A, 0x2E, 0x35, 0xFF);
        ButtonDarkIcon = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
        KeyText = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
        ShadowBase = C2D_Color32(0x1E, 0x28, 0x38, 0xFF);
        TagText = C2D_Color32(0xB0, 0x7A, 0x0A, 0xFF);
        Danger = C2D_Color32(0xC0, 0x39, 0x2B, 0xFF);
        Dim = C2D_Color32(0x2A, 0x2E, 0x35, 0x80);
        buildDots(C2D_Color32(0xD6, 0xDB, 0xE3, 0xFF));
    }
}

namespace {

C2D_TextBuf textBuf = nullptr;
C2D_TextBuf measureBuf = nullptr;
// Two sizes of each weight: the 24 pt faces for titles, the 12 pt faces for
// everything under 14 px so small text is not a blurry downsample.
C2D_Font fontRegular = nullptr;
C2D_Font fontBold = nullptr;
C2D_Font fontRegularSmall = nullptr;
C2D_Font fontBoldSmall = nullptr;
float lineHeightRegular = 30.0f;
float lineHeightBold = 30.0f;
float lineHeightRegularSmall = 15.0f;
float lineHeightBoldSmall = 15.0f;
constexpr float SmallCutoff = 14.0f;
// Debug switches, read once: files under sdmc:/3ds/3dsFM/ named
// debug-c2dcircles, debug-notext and debug-nodots.
bool useC2dCircles = false;
bool skipText = false;
bool skipDots = false;
bool useSystemFont = false;
float screenW = 400.0f;
float screenH = 240.0f;

struct MarqueeState {
    std::string text;
    float offset = 0.0f;
    int hold = 0;
    uint32_t seen = 0;
};
std::map<std::string, MarqueeState> marquees;
uint32_t marqueeFrame = 0;
// `size` is meant as the visual height of a line of text (cap height plus a
// little), so the scale is derived from each font's own line height.
// Every size the screens ask for is enlarged by TextScale: the layouts were
// drawn against 2x mockups, and on a console the text came out too small
// to read comfortably at arm's length.
constexpr float TextScale = 1.2f;
C2D_Font fontFor(Weight weight, float size)
{
    if (useSystemFont) return nullptr;
    if (size * TextScale < SmallCutoff) return weight == Weight::Bold ? fontBoldSmall : fontRegularSmall;
    return weight == Weight::Bold ? fontBold : fontRegular;
}
float lineHeightFor(Weight weight, float size)
{
    if (useSystemFont) return 30.0f;
    if (size * TextScale < SmallCutoff) return weight == Weight::Bold ? lineHeightBoldSmall : lineHeightRegularSmall;
    return weight == Weight::Bold ? lineHeightBold : lineHeightRegular;
}
float scaleFor(float size, Weight weight)
{
    return size * TextScale * 1.3f / lineHeightFor(weight, size);
}

std::string ellipsize(const std::string &s, float size, float maxWidth, Weight weight)
{
    if (maxWidth <= 0.0f || textWidth(s, size, weight) <= maxWidth) return s;
    std::string out = s;
    while (!out.empty()) {
        // Cut on a UTF-8 boundary.
        do {
            out.pop_back();
        } while (!out.empty() && (static_cast<unsigned char>(out.back()) & 0xC0) == 0x80);
        const std::string candidate = out + "...";
        if (textWidth(candidate, size, weight) <= maxWidth) return candidate;
    }
    return "...";
}

} // namespace

u32 withAlpha(u32 color, u8 alpha)
{
    return (color & 0x00FFFFFF) | (u32(alpha) << 24);
}

u32 mix(u32 a, u32 b, float t)
{
    auto ch = [&](int shift) {
        const float x = float((a >> shift) & 0xFF);
        const float y = float((b >> shift) & 0xFF);
        return u32(x + (y - x) * t) & 0xFF;
    };
    return ch(0) | (ch(8) << 8) | (ch(16) << 16) | (ch(24) << 24);
}

void init()
{
    useC2dCircles = platform::fileExists("sdmc:/3ds/3dsFM/debug-c2dcircles");
    skipText = platform::fileExists("sdmc:/3ds/3dsFM/debug-notext");
    skipDots = platform::fileExists("sdmc:/3ds/3dsFM/debug-nodots");
    useSystemFont = platform::fileExists("sdmc:/3ds/3dsFM/debug-sysfont");
    textBuf = C2D_TextBufNew(8192);
    measureBuf = C2D_TextBufNew(512);
    fontRegular = C2D_FontLoad("romfs:/fonts/medium24.bcfnt");
    fontBold = C2D_FontLoad("romfs:/fonts/bold24.bcfnt");
    fontRegularSmall = C2D_FontLoad("romfs:/fonts/medium12.bcfnt");
    fontBoldSmall = C2D_FontLoad("romfs:/fonts/bold12.bcfnt");
    if (fontBold == nullptr) fontBold = fontRegular;
    if (fontRegularSmall == nullptr) fontRegularSmall = fontRegular;
    if (fontBoldSmall == nullptr) fontBoldSmall = fontBold;
    auto measureLine = [](C2D_Font font) {
        C2D_TextBufClear(measureBuf);
        C2D_Text t;
        C2D_TextFontParse(&t, font, measureBuf, "Hg");
        float h = 30.0f;
        C2D_TextGetDimensions(&t, 1.0f, 1.0f, nullptr, &h);
        return h > 1.0f ? h : 30.0f;
    };
    lineHeightRegular = measureLine(fontRegular);
    lineHeightBold = measureLine(fontBold);
    lineHeightRegularSmall = measureLine(fontRegularSmall);
    lineHeightBoldSmall = measureLine(fontBoldSmall);
    setTheme(themeDark);
}

void shutdown()
{
    if (dotsReady) C3D_TexDelete(&dotTex);
    if (fontBoldSmall != nullptr && fontBoldSmall != fontBold) C2D_FontFree(fontBoldSmall);
    if (fontRegularSmall != nullptr && fontRegularSmall != fontRegular) C2D_FontFree(fontRegularSmall);
    if (fontBold != nullptr && fontBold != fontRegular) C2D_FontFree(fontBold);
    if (fontRegular != nullptr) C2D_FontFree(fontRegular);
    C2D_TextBufDelete(textBuf);
    C2D_TextBufDelete(measureBuf);
}

void beginFrame()
{
    C2D_TextBufClear(textBuf);
}

void beginScreen(float width, float height)
{
    screenW = width;
    screenH = height;
}

void clipBegin(float x, float y, float w, float h)
{
    C2D_Flush();
    // Screen targets are rotated: the scissor is given in framebuffer space.
    C3D_SetScissor(GPU_SCISSOR_NORMAL, u32(std::max(0.0f, screenH - (y + h))), u32(std::max(0.0f, screenW - (x + w))), u32(std::min(screenH, screenH - y)), u32(std::min(screenW, screenW - x)));
}

void clipEnd()
{
    C2D_Flush();
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
}

void marqueeTick()
{
    ++marqueeFrame;
    // Forget slots that were not drawn for a while.
    for (auto it = marquees.begin(); it != marquees.end();) {
        if (marqueeFrame - it->second.seen > 120) it = marquees.erase(it); else ++it;
    }
}

void marquee(const char *slot, const std::string &s, float x, float y, float size, u32 color, float maxWidth, Weight weight)
{
    if (s.empty()) return;
    const float w = textWidth(s, size, weight);
    if (w <= maxWidth) {
        text(s, x, y, size, color, Align::Left, 0.0f, weight);
        return;
    }
    MarqueeState &m = marquees[slot];
    if (m.text != s) {
        m.text = s;
        m.offset = 0.0f;
        m.hold = 90;
    }
    const float gap = 64.0f;
    if (m.seen != marqueeFrame) {
        m.seen = marqueeFrame;
        if (m.hold > 0) {
            --m.hold;
        } else {
            m.offset += 0.45f;
            if (m.offset >= w + gap) {
                m.offset = 0.0f;
                m.hold = 60;
            }
        }
    }
    clipBegin(x, y - 4.0f, maxWidth, size + 10.0f);
    text(s, x - m.offset, y, size, color, Align::Left, 0.0f, weight);
    text(s, x - m.offset + w + gap, y, size, color, Align::Left, 0.0f, weight);
    clipEnd();
}

float textWidth(const std::string &s, float size, Weight weight)
{
    C2D_TextBufClear(measureBuf);
    C2D_Text t;
    C2D_TextFontParse(&t, fontFor(weight, size), measureBuf, s.c_str());
    float w = 0.0f;
    C2D_TextGetDimensions(&t, scaleFor(size, weight), scaleFor(size, weight), &w, nullptr);
    return w;
}

float fitSize(const std::string &s, float size, float minSize, float maxWidth, Weight weight)
{
    float current = size;
    while (current > minSize && textWidth(s, current, weight) > maxWidth) current -= 1.0f;
    return current < minSize ? minSize : current;
}

void text(const std::string &s, float x, float y, float size, u32 color, Align align, float maxWidth, Weight weight)
{
    if (s.empty() || skipText) return;
    const std::string shown = ellipsize(s, size, maxWidth, weight);
    C2D_Text t;
    C2D_TextFontParse(&t, fontFor(weight, size), textBuf, shown.c_str());
    C2D_TextOptimize(&t);
    const float scale = scaleFor(size, weight);
    float drawX = x;
    if (align != Align::Left) {
        float w = 0.0f;
        C2D_TextGetDimensions(&t, scale, scale, &w, nullptr);
        drawX = align == Align::Center ? x - w / 2.0f : x - w;
    }
    // Centre the glyphs on the requested pixel height.
    const float lineHeight = lineHeightFor(weight, size) * scale;
    const float top = y - (lineHeight - size * TextScale) / 2.0f;
    C2D_DrawText(&t, C2D_WithColor, drawX, top, 0.5f, scale, scale, color);
}

void rect(float x, float y, float w, float h, u32 color)
{
    C2D_DrawRectSolid(x, y, 0.5f, w, h, color);
}

void roundRect(float x, float y, float w, float h, float r, u32 color)
{
    if (r <= 0.5f) {
        rect(x, y, w, h, color);
        return;
    }
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    rect(x + r, y, w - 2 * r, h, color);
    rect(x, y + r, r, h - 2 * r, color);
    rect(x + w - r, y + r, r, h - 2 * r, color);
    circle(x + r, y + r, r, color);
    circle(x + w - r, y + r, r, color);
    circle(x + r, y + h - r, r, color);
    circle(x + w - r, y + h - r, r, color);
}

void roundCorners(float x, float y, float w, float h, float r, u32 top, u32 bottom)
{
    if (r <= 0.5f) return;
    const int segments = r < 6.0f ? 4 : 6;
    // Each corner: a fan from the square corner over the arc.
    const float corners[4][2] = {{x, y}, {x + w, y}, {x, y + h}, {x + w, y + h}};
    const float centers[4][2] = {{x + r, y + r}, {x + w - r, y + r}, {x + r, y + h - r}, {x + w - r, y + h - r}};
    const float startAngle[4] = {3.14159f, 4.71239f, 1.5708f, 0.0f};
    for (int c = 0; c < 4; ++c) {
        const u32 color = c < 2 ? top : bottom;
        float px = centers[c][0] + std::cos(startAngle[c]) * r;
        float py = centers[c][1] + std::sin(startAngle[c]) * r;
        for (int i = 1; i <= segments; ++i) {
            const float a = startAngle[c] + 1.5708f * float(i) / float(segments);
            const float nx = centers[c][0] + std::cos(a) * r;
            const float ny = centers[c][1] + std::sin(a) * r;
            C2D_DrawTriangle(corners[c][0], corners[c][1], color, px, py, color, nx, ny, color, 0.5f);
            px = nx;
            py = ny;
        }
    }
}

void circle(float cx, float cy, float r, u32 color)
{
    if (r <= 0.0f) return;
    if (useC2dCircles) {
        C2D_DrawCircleSolid(cx, cy, 0.5f, r, color);
        return;
    }
    // A triangle fan rather than Citro2D's circle primitive, which some
    // emulator renderers do not draw. Segment count follows the radius.
    const int segments = r < 3.0f ? 8 : r < 8.0f ? 14 : r < 16.0f ? 22 : 32;
    float px = cx + r;
    float py = cy;
    for (int i = 1; i <= segments; ++i) {
        const float a = 6.2831853f * float(i) / float(segments);
        const float x = cx + std::cos(a) * r;
        const float y = cy + std::sin(a) * r;
        C2D_DrawTriangle(cx, cy, color, px, py, color, x, y, color, 0.5f);
        px = x;
        py = y;
    }
}

void ring(float cx, float cy, float r, float thickness, u32 color, u32 inside)
{
    circle(cx, cy, r, color);
    circle(cx, cy, r - thickness, inside);
}

void line(float x0, float y0, float x1, float y1, float thickness, u32 color)
{
    C2D_DrawLine(x0, y0, color, x1, y1, color, thickness, 0.5f);
}

void gradientV(float x, float y, float w, float h, u32 top, u32 bottom)
{
    C2D_DrawRectangle(x, y, 0.5f, w, h, top, top, bottom, bottom);
}

void roundRectGradient(float x, float y, float w, float h, float r, u32 top, u32 bottom)
{
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    if (r < 1.0f) {
        gradientV(x, y, w, h, top, bottom);
        return;
    }
    // One-pixel strips through the corner zones, the plain gradient between.
    const int rows = int(r + 0.5f);
    for (int i = 0; i < rows; ++i) {
        const float dy = r - (i + 0.5f);
        const float inset = r - std::sqrt(std::max(0.0f, r * r - dy * dy));
        const float ty = float(i);
        const u32 cTop = mix(top, bottom, ty / h);
        const u32 cNext = mix(top, bottom, (ty + 1.0f) / h);
        gradientV(x + inset, y + ty, w - 2 * inset, 1.0f, cTop, cNext);
        const float by = h - 1.0f - ty;
        const u32 cBot = mix(top, bottom, by / h);
        const u32 cBotNext = mix(top, bottom, (by + 1.0f) / h);
        gradientV(x + inset, y + by, w - 2 * inset, 1.0f, cBot, cBotNext);
    }
    const u32 cMidTop = mix(top, bottom, float(rows) / h);
    const u32 cMidBottom = mix(top, bottom, (h - float(rows)) / h);
    gradientV(x, y + rows, w, h - 2 * rows, cMidTop, cMidBottom);
}

void gradientH(float x, float y, float w, float h, u32 left, u32 right)
{
    C2D_DrawRectangle(x, y, 0.5f, w, h, left, right, left, right);
}

void dots(float x, float y, float w, float h)
{
    if (!dotsReady || skipDots) return;
    Tex3DS_SubTexture sub = dotSub;
    sub.width = u16(w);
    sub.height = u16(h);
    sub.right = w / 8.0f;
    sub.bottom = 1.0f - h / 8.0f;
    C2D_Image img{&dotTex, &sub};
    C2D_DrawImageAt(img, x, y, 0.5f);
}

void icon(Icon which, float cx, float cy, float s, u32 color, u32 bg)
{
    const float h = s / 2.0f;   // half size
    const float t = s >= 16 ? 2.2f : s >= 12 ? 1.8f : 1.5f; // stroke
    switch (which) {
    case Icon::Home:
        C2D_DrawTriangle(cx - h, cy - h * 0.1f, color, cx, cy - h, color, cx + h, cy - h * 0.1f, color, 0.5f);
        rect(cx - h * 0.72f, cy - h * 0.15f, h * 1.44f, h * 1.1f, color);
        rect(cx - h * 0.2f, cy + h * 0.35f, h * 0.4f, h * 0.6f, bg);
        break;
    case Icon::Note:
        circle(cx - h * 0.35f, cy + h * 0.55f, h * 0.42f, color);
        rect(cx + h * 0.0f, cy - h * 0.85f, t, h * 1.4f, color);
        rect(cx + h * 0.0f, cy - h * 0.85f, h * 0.75f, t * 1.3f, color);
        break;
    case Icon::Grid: {
        const float b = h * 0.78f;
        roundRect(cx - b, cy - b, b * 0.9f, b * 0.9f, 1.5f, color);
        roundRect(cx + b * 0.1f, cy - b, b * 0.9f, b * 0.9f, 1.5f, color);
        roundRect(cx - b, cy + b * 0.1f, b * 0.9f, b * 0.9f, 1.5f, color);
        roundRect(cx + b * 0.1f, cy + b * 0.1f, b * 0.9f, b * 0.9f, 1.5f, color);
        break;
    }
    case Icon::List:
        rect(cx - h * 0.8f, cy - h * 0.65f, h * 1.6f, t, color);
        rect(cx - h * 0.8f, cy - t / 2, h * 1.6f, t, color);
        rect(cx - h * 0.8f, cy + h * 0.65f - t, h * 1.0f, t, color);
        break;
    case Icon::Queue:
        rect(cx - h * 0.85f, cy - h * 0.6f, h * 1.0f, t, color);
        rect(cx - h * 0.85f, cy - t / 2, h * 1.0f, t, color);
        rect(cx - h * 0.85f, cy + h * 0.6f - t, h * 0.6f, t, color);
        circle(cx + h * 0.45f, cy + h * 0.45f, h * 0.3f, color);
        rect(cx + h * 0.7f, cy - h * 0.7f, t, h * 1.15f, color);
        break;
    case Icon::Lyrics:
        rect(cx - h * 0.8f, cy - h * 0.65f, h * 1.6f, t, color);
        rect(cx - h * 0.8f, cy - t / 2, h * 1.1f, t, color);
        rect(cx - h * 0.8f, cy + h * 0.65f - t, h * 0.7f, t, color);
        break;
    case Icon::Search:
        ring(cx - h * 0.15f, cy - h * 0.15f, h * 0.62f, t, color, bg);
        line(cx + h * 0.3f, cy + h * 0.3f, cx + h * 0.85f, cy + h * 0.85f, t * 1.2f, color);
        break;
    case Icon::Gear:
        for (int i = 0; i < 8; ++i) {
            const float a = float(i) * 3.14159f / 4.0f;
            circle(cx + std::cos(a) * h * 0.72f, cy + std::sin(a) * h * 0.72f, h * 0.22f, color);
        }
        ring(cx, cy, h * 0.6f, h * 0.28f, color, bg);
        break;
    case Icon::Play:
        C2D_DrawTriangle(cx - h * 0.6f, cy - h * 0.8f, color, cx - h * 0.6f, cy + h * 0.8f, color, cx + h * 0.8f, cy, color, 0.5f);
        break;
    case Icon::Pause:
        rect(cx - h * 0.65f, cy - h * 0.75f, h * 0.45f, h * 1.5f, color);
        rect(cx + h * 0.2f, cy - h * 0.75f, h * 0.45f, h * 1.5f, color);
        break;
    case Icon::Prev:
        rect(cx - h * 0.8f, cy - h * 0.7f, h * 0.3f, h * 1.4f, color);
        C2D_DrawTriangle(cx + h * 0.8f, cy - h * 0.7f, color, cx + h * 0.8f, cy + h * 0.7f, color, cx - h * 0.4f, cy, color, 0.5f);
        break;
    case Icon::Next:
        rect(cx + h * 0.5f, cy - h * 0.7f, h * 0.3f, h * 1.4f, color);
        C2D_DrawTriangle(cx - h * 0.8f, cy - h * 0.7f, color, cx - h * 0.8f, cy + h * 0.7f, color, cx + h * 0.4f, cy, color, 0.5f);
        break;
    case Icon::Shuffle:
        line(cx - h * 0.8f, cy - h * 0.5f, cx + h * 0.6f, cy + h * 0.55f, t, color);
        line(cx - h * 0.8f, cy + h * 0.55f, cx + h * 0.6f, cy - h * 0.5f, t, color);
        C2D_DrawTriangle(cx + h * 0.45f, cy - h * 0.85f, color, cx + h * 0.95f, cy - h * 0.5f, color, cx + h * 0.45f, cy - h * 0.15f, color, 0.5f);
        C2D_DrawTriangle(cx + h * 0.45f, cy + h * 0.2f, color, cx + h * 0.95f, cy + h * 0.55f, color, cx + h * 0.45f, cy + h * 0.9f, color, 0.5f);
        break;
    case Icon::Repeat:
    case Icon::RepeatOne:
        ring(cx, cy, h * 0.7f, t, color, bg);
        rect(cx - h * 0.1f, cy - h * 0.95f, h * 1.1f, h * 0.5f, bg);
        C2D_DrawTriangle(cx + h * 0.05f, cy - h * 1.0f, color, cx + h * 0.55f, cy - h * 0.7f, color, cx + h * 0.05f, cy - h * 0.4f, color, 0.5f);
        if (which == Icon::RepeatOne) rect(cx - t / 2, cy - h * 0.3f, t, h * 0.6f, color);
        break;
    case Icon::Headphones:
        ring(cx, cy + h * 0.1f, h * 0.75f, t, color, bg);
        rect(cx - h * 0.9f, cy + h * 0.1f, h * 1.8f, h * 0.9f, bg);
        roundRect(cx - h * 0.85f, cy, h * 0.32f, h * 0.65f, 1.5f, color);
        roundRect(cx + h * 0.53f, cy, h * 0.32f, h * 0.65f, 1.5f, color);
        break;
    case Icon::Wifi:
        // Three rising bars, like the HOME Menu's signal meter.
        for (int bar = 0; bar < 3; ++bar) {
            const float bh = h * (0.6f + bar * 0.5f);
            roundRect(cx - h * 0.85f + bar * h * 0.65f, cy + h * 0.8f - bh, h * 0.45f, bh, 1.0f, color);
        }
        (void)bg;
        break;
    case Icon::Battery:
        roundRect(cx - h * 0.95f, cy - h * 0.45f, h * 1.7f, h * 0.9f, 1.5f, color);
        roundRect(cx - h * 0.95f + t, cy - h * 0.45f + t, h * 1.7f - 2 * t, h * 0.9f - 2 * t, 1.0f, bg);
        rect(cx + h * 0.8f, cy - h * 0.2f, h * 0.2f, h * 0.4f, color);
        break;
    case Icon::Clock:
        ring(cx, cy, h * 0.8f, t, color, bg);
        line(cx, cy, cx, cy - h * 0.45f, t, color);
        line(cx, cy, cx + h * 0.35f, cy + h * 0.2f, t, color);
        break;
    case Icon::ChevronRight:
        line(cx - h * 0.3f, cy - h * 0.6f, cx + h * 0.3f, cy, t, color);
        line(cx + h * 0.3f, cy, cx - h * 0.3f, cy + h * 0.6f, t, color);
        break;
    case Icon::ChevronLeft:
    case Icon::Back:
        line(cx + h * 0.3f, cy - h * 0.6f, cx - h * 0.3f, cy, t, color);
        line(cx - h * 0.3f, cy, cx + h * 0.3f, cy + h * 0.6f, t, color);
        break;
    case Icon::Close:
        line(cx - h * 0.6f, cy - h * 0.6f, cx + h * 0.6f, cy + h * 0.6f, t, color);
        line(cx + h * 0.6f, cy - h * 0.6f, cx - h * 0.6f, cy + h * 0.6f, t, color);
        break;
    case Icon::Plus:
        rect(cx - h * 0.7f, cy - t / 2, h * 1.4f, t, color);
        rect(cx - t / 2, cy - h * 0.7f, t, h * 1.4f, color);
        break;
    case Icon::Minus:
        rect(cx - h * 0.7f, cy - t / 2, h * 1.4f, t, color);
        break;
    case Icon::Trash:
        rect(cx - h * 0.8f, cy - h * 0.6f, h * 1.6f, t, color);
        rect(cx - h * 0.25f, cy - h * 0.85f, h * 0.5f, t, color);
        roundRect(cx - h * 0.6f, cy - h * 0.45f, h * 1.2f, h * 1.3f, 1.5f, color);
        rect(cx - h * 0.6f + t, cy - h * 0.45f + t, h * 1.2f - 2 * t, h * 1.3f - 2 * t, bg);
        rect(cx - h * 0.2f, cy - h * 0.2f, t, h * 0.8f, color);
        rect(cx + h * 0.2f - t, cy - h * 0.2f, t, h * 0.8f, color);
        break;
    case Icon::Grip:
        for (int r = -1; r <= 1; ++r) {
            circle(cx - h * 0.3f, cy + r * h * 0.55f, h * 0.17f, color);
            circle(cx + h * 0.3f, cy + r * h * 0.55f, h * 0.17f, color);
        }
        break;
    case Icon::Keyboard:
        roundRect(cx - h * 0.95f, cy - h * 0.55f, h * 1.9f, h * 1.1f, 1.5f, color);
        rect(cx - h * 0.95f + t, cy - h * 0.55f + t, h * 1.9f - 2 * t, h * 1.1f - 2 * t, bg);
        for (int i = 0; i < 4; ++i) rect(cx - h * 0.7f + i * h * 0.4f, cy - h * 0.25f, t, t, color);
        rect(cx - h * 0.5f, cy + h * 0.1f, h * 1.0f, t, color);
        break;
    case Icon::Bars:
        rect(cx - h * 0.8f, cy - h * 0.1f, t * 1.2f, h * 0.9f, color);
        rect(cx - h * 0.3f, cy - h * 0.8f, t * 1.2f, h * 1.6f, color);
        rect(cx + h * 0.2f, cy - h * 0.4f, t * 1.2f, h * 1.2f, color);
        rect(cx + h * 0.7f, cy - h * 0.6f, t * 1.2f, h * 1.4f, color);
        break;
    case Icon::Person:
        circle(cx, cy - h * 0.4f, h * 0.35f, color);
        circle(cx, cy + h * 0.75f, h * 0.7f, color);
        rect(cx - h, cy + h * 0.75f, h * 2, h, bg);
        break;
    case Icon::Check:
        line(cx - h * 0.65f, cy, cx - h * 0.15f, cy + h * 0.5f, t * 1.3f, color);
        line(cx - h * 0.15f, cy + h * 0.5f, cx + h * 0.7f, cy - h * 0.5f, t * 1.3f, color);
        break;
    case Icon::Retry:
        ring(cx, cy, h * 0.7f, t, color, bg);
        rect(cx, cy - h * 0.95f, h, h * 0.6f, bg);
        C2D_DrawTriangle(cx + h * 0.0f, cy - h * 0.95f, color, cx + h * 0.55f, cy - h * 0.6f, color, cx + h * 0.0f, cy - h * 0.25f, color, 0.5f);
        break;
    case Icon::Album:
        ring(cx, cy, h * 0.85f, h * 0.45f, color, bg);
        circle(cx, cy, h * 0.18f, color);
        break;
    }
}

void keyCap(const char *letter, float cx, float cy, u32 color)
{
    circle(cx, cy, 5.5f, color);
    text(letter, cx, cy - 4.0f, 7.5f, KeyText, Align::Center, 0.0f, Weight::Bold);
}

void legend(const LegendItem *items, int count, float rightX, float y)
{
    float x = rightX;
    for (int i = count - 1; i >= 0; --i) {
        const float w = textWidth(items[i].label, 9.0f);
        x -= w;
        text(items[i].label, x, y - 5.0f, 9.0f, Meta);
        x -= 8.0f;
        keyCap(items[i].key, x, y, Meta);
        x -= 14.0f;
    }
}

void roundButton(float cx, float cy, float r, Icon which, bool dark, bool focused, u32 iconColor)
{
    if (focused) circle(cx, cy, r + 3.0f, Blue);
    circle(cx, cy + 0.8f, r, withAlpha(ShadowBase, 0x1E));
    circle(cx, cy, r, dark ? ButtonDark : Sheet);
    const u32 fg = iconColor != 0 ? iconColor : dark ? ButtonDarkIcon : Ink;
    icon(which, cx, cy, r * 1.15f, fg, dark ? ButtonDark : Sheet);
}

void pill(const std::string &label, float x, float y, u32 bg, u32 fg, float size)
{
    const float w = textWidth(label, size, Weight::Bold) + 12.0f;
    roundRect(x, y, w, 15.0f, 7.5f, bg);
    text(label, x + 6.0f, y + 3.0f, size, fg, Align::Left, 0.0f, Weight::Bold);
}

void tag(const std::string &label, float x, float y, u32 outline, u32 fg)
{
    const float w = textWidth(label, 8.0f, Weight::Bold) + 12.0f;
    roundRect(x, y, w, 14.0f, 7.0f, outline);
    roundRect(x + 1.0f, y + 1.0f, w - 2.0f, 12.0f, 6.0f, Sheet);
    text(label, x + 6.0f, y + 3.0f, 8.0f, fg, Align::Left, 0.0f, Weight::Bold);
}

void progressBar(float x, float y, float w, float h, float fraction, u32 track)
{
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;
    roundRect(x, y, w, h, h / 2.0f, track);
    if (fraction > 0.0f) {
        const float fw = w * fraction < h ? h : w * fraction;
        gradientV(x + h / 2.0f, y, fw - h, h, BlueLight, BlueDeep);
        circle(x + h / 2.0f, y + h / 2.0f, h / 2.0f, BlueLight);
        circle(x + fw - h / 2.0f, y + h / 2.0f, h / 2.0f, BlueDeep);
    }
}

void equaliser(float x, float y, float level, u32 color, int frame)
{
    // Three bars that bob with the level, phase-shifted so they look alive.
    for (int i = 0; i < 3; ++i) {
        const float wobble = 0.55f + 0.45f * std::sin(float(frame) * 0.25f + i * 2.1f);
        const float h = 2.0f + 6.0f * (0.3f + 0.7f * level) * wobble;
        rect(x + i * 3.0f, y + 8.0f - h, 2.0f, h, color);
    }
}

void toggle(float x, float y, bool on)
{
    roundRect(x, y, 20.0f, 11.0f, 5.5f, on ? BlueDeep : Line);
    circle(on ? x + 14.5f : x + 5.5f, y + 5.5f, 4.0f, Sheet);
}

bool hit(float x, float y, float w, float h, float px, float py)
{
    return px >= x && px < x + w && py >= y && py < y + h;
}

} // namespace ui
