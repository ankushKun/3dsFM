#pragma once

#include <3ds.h>
#include <citro2d.h>

#include <cstdint>
#include <string>
#include <vector>

// Album art as GPU textures: JPEG or PNG in, a 128x128 texture out, with
// the picture's two main colors for the visualizer.
namespace art {

struct Image {
    C3D_Tex tex{};
    Tex3DS_SubTexture sub{};
    bool ready = false;
    bool failed = false;
    uint32_t colorA = 0xFF8FD1FF; // ABGR as C2D_Color32 produces
    uint32_t colorB = 0xFF3B97EA;
    uint32_t lastUsedFrame = 0;
};

bool loadFromBytes(const uint8_t *data, size_t size, Image &out);
bool loadFromFile(const std::string &path, Image &out);
void release(Image &image);

// Draws the image scaled to w x h at x, y.
void draw(const Image &image, float x, float y, float w, float h, float depth = 0.5f);

// Writes RGBA8 pixels (width x height, powers of two) into a texture.
bool makeTexture(const uint8_t *rgba, int width, int height, C3D_Tex &tex, Tex3DS_SubTexture &sub);

// Background decoding. Decoding a cover takes around 100 ms on the console,
// so requests are queued to a low-priority thread and finished pictures are
// turned into textures on the main thread, one per frame.
void startLoader();
void stopLoader();
// Keeps the loader idle while the console is away in the HOME Menu or asleep.
void setPaused(bool paused);
// `embedded` means the picture is inside the audio file at `path`. A key
// asked for again moves to the front of the line.
void request(const std::string &key, const std::string &path, bool embedded);
// The keys on screen right now. The loader serves these before anything
// else, newest first, and drops the oldest requests nobody is looking at
// once the line grows long.
void setWanted(const std::vector<std::string> &keys);
// Takes one finished decode, if any. Returns false when nothing is ready.
bool takeFinished(std::string &key, Image &out);

} // namespace art
