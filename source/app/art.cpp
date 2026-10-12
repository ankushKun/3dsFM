#include "art.hpp"

#include "platform.hpp"
#include "tags.hpp"

#include <3ds.h>

#include <deque>
#include <mutex>

#include <jpeglib.h>
#include <png.h>

#include <csetjmp>
#include <cstring>

namespace art {

namespace {

constexpr int Size = 256;

struct JpegError {
    jpeg_error_mgr pub;
    jmp_buf jump;
};

void jpegErrorExit(j_common_ptr info)
{
    JpegError *err = reinterpret_cast<JpegError *>(info->err);
    longjmp(err->jump, 1);
}

// Decodes a JPEG into RGB, letting libjpeg downscale big pictures on the way.
bool decodeJpeg(const uint8_t *data, size_t size, std::vector<uint8_t> &rgb, int &width, int &height)
{
    jpeg_decompress_struct cinfo{};
    JpegError err{};
    cinfo.err = jpeg_std_error(&err.pub);
    err.pub.error_exit = jpegErrorExit;
    if (setjmp(err.jump)) {
        jpeg_destroy_decompress(&cinfo);
        return false;
    }
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, const_cast<unsigned char *>(data), static_cast<unsigned long>(size));
    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&cinfo);
        return false;
    }
    cinfo.out_color_space = JCS_RGB;
    cinfo.scale_num = 1;
    cinfo.scale_denom = 1;
    while (cinfo.scale_denom < 8 && int(cinfo.image_width) / int(cinfo.scale_denom * 2) >= Size && int(cinfo.image_height) / int(cinfo.scale_denom * 2) >= Size) {
        cinfo.scale_denom *= 2;
    }
    cinfo.dct_method = JDCT_IFAST;
    cinfo.do_fancy_upsampling = FALSE;
    jpeg_start_decompress(&cinfo);
    width = int(cinfo.output_width);
    height = int(cinfo.output_height);
    if (cinfo.output_components != 3 || width <= 0 || height <= 0 || width > 4096 || height > 4096) {
        jpeg_destroy_decompress(&cinfo);
        return false;
    }
    rgb.resize(size_t(width) * height * 3);
    while (cinfo.output_scanline < cinfo.output_height) {
        JSAMPROW row = rgb.data() + size_t(cinfo.output_scanline) * width * 3;
        jpeg_read_scanlines(&cinfo, &row, 1);
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return true;
}

struct PngReader {
    const uint8_t *data;
    size_t size;
    size_t pos;
};

void pngRead(png_structp png, png_bytep out, png_size_t n)
{
    PngReader *r = static_cast<PngReader *>(png_get_io_ptr(png));
    if (r->pos + n > r->size) {
        png_error(png, "short read");
        return;
    }
    std::memcpy(out, r->data + r->pos, n);
    r->pos += n;
}

bool decodePng(const uint8_t *data, size_t size, std::vector<uint8_t> &rgb, int &width, int &height)
{
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (png == nullptr) return false;
    png_infop info = png_create_info_struct(png);
    if (info == nullptr) {
        png_destroy_read_struct(&png, nullptr, nullptr);
        return false;
    }
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        return false;
    }
    PngReader reader{data, size, 0};
    png_set_read_fn(png, &reader, pngRead);
    png_read_info(png, info);
    width = int(png_get_image_width(png, info));
    height = int(png_get_image_height(png, info));
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096) {
        png_destroy_read_struct(&png, &info, nullptr);
        return false;
    }
    const int colorType = png_get_color_type(png, info);
    const int bitDepth = png_get_bit_depth(png, info);
    if (colorType == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (bitDepth == 16) png_set_strip_16(png);
    if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);
    png_set_strip_alpha(png);
    png_read_update_info(png, info);
    rgb.resize(size_t(width) * height * 3);
    std::vector<png_bytep> rows(height);
    for (int y = 0; y < height; ++y) rows[y] = rgb.data() + size_t(y) * width * 3;
    png_read_image(png, rows.data());
    png_destroy_read_struct(&png, &info, nullptr);
    return true;
}

// Box-filters an RGB picture down (or nearest-neighbours it up) to a square.
void resample(const std::vector<uint8_t> &rgb, int width, int height, std::vector<uint8_t> &rgba)
{
    rgba.assign(size_t(Size) * Size * 4, 255);
    for (int y = 0; y < Size; ++y) {
        const int sy0 = y * height / Size;
        int sy1 = (y + 1) * height / Size;
        if (sy1 <= sy0) sy1 = sy0 + 1;
        for (int x = 0; x < Size; ++x) {
            const int sx0 = x * width / Size;
            int sx1 = (x + 1) * width / Size;
            if (sx1 <= sx0) sx1 = sx0 + 1;
            uint32_t r = 0;
            uint32_t g = 0;
            uint32_t b = 0;
            uint32_t n = 0;
            // Sample at most 4x4 source pixels per output pixel to stay cheap.
            const int stepY = (sy1 - sy0 + 3) / 4;
            const int stepX = (sx1 - sx0 + 3) / 4;
            for (int sy = sy0; sy < sy1 && sy < height; sy += stepY) {
                for (int sx = sx0; sx < sx1 && sx < width; sx += stepX) {
                    const uint8_t *p = rgb.data() + (size_t(sy) * width + sx) * 3;
                    r += p[0];
                    g += p[1];
                    b += p[2];
                    ++n;
                }
            }
            uint8_t *o = rgba.data() + (size_t(y) * Size + x) * 4;
            o[0] = uint8_t(r / n);
            o[1] = uint8_t(g / n);
            o[2] = uint8_t(b / n);
            o[3] = 255;
        }
    }
}

// Picks two colors: the average, and the most saturated well-populated bucket.
void pickColors(const std::vector<uint8_t> &rgba, uint32_t &a, uint32_t &b)
{
    uint64_t sr = 0;
    uint64_t sg = 0;
    uint64_t sb = 0;
    uint32_t count[64] = {};
    uint64_t bucketR[64] = {};
    uint64_t bucketG[64] = {};
    uint64_t bucketB[64] = {};
    const size_t n = size_t(Size) * Size;
    for (size_t i = 0; i < n; ++i) {
        const uint8_t *p = rgba.data() + i * 4;
        sr += p[0];
        sg += p[1];
        sb += p[2];
        const int idx = (p[0] >> 6) * 16 + (p[1] >> 6) * 4 + (p[2] >> 6);
        ++count[idx];
        bucketR[idx] += p[0];
        bucketG[idx] += p[1];
        bucketB[idx] += p[2];
    }
    const uint8_t ar = uint8_t(sr / n);
    const uint8_t ag = uint8_t(sg / n);
    const uint8_t ab = uint8_t(sb / n);
    a = C2D_Color32(ar, ag, ab, 255);
    int best = -1;
    float bestScore = 0.0f;
    for (int i = 0; i < 64; ++i) {
        if (count[i] < n / 50) continue;
        const float r = float(bucketR[i] / count[i]);
        const float g = float(bucketG[i] / count[i]);
        const float bl = float(bucketB[i] / count[i]);
        const float mx = std::max(r, std::max(g, bl));
        const float mn = std::min(r, std::min(g, bl));
        if (mx < 40.0f || mx > 245.0f) continue;
        const float score = (mx - mn) * (1.0f + count[i] * 4.0f / n);
        if (score > bestScore) {
            bestScore = score;
            best = i;
        }
    }
    if (best >= 0) {
        b = C2D_Color32(uint8_t(bucketR[best] / count[best]), uint8_t(bucketG[best] / count[best]), uint8_t(bucketB[best] / count[best]), 255);
    } else {
        b = C2D_Color32(uint8_t(ar * 0.6f), uint8_t(ag * 0.6f), uint8_t(ab * 0.6f), 255);
    }
}

} // namespace

bool makeTexture(const uint8_t *rgba, int width, int height, C3D_Tex &tex, Tex3DS_SubTexture &sub)
{
    if (!C3D_TexInit(&tex, u16(width), u16(height), GPU_RGBA8)) return false;
    uint32_t *dst = static_cast<uint32_t *>(tex.data);
    // The PICA200 stores textures as 8x8 tiles in Morton order.
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int tileX = x >> 3;
            const int tileY = y >> 3;
            const int inX = x & 7;
            const int inY = y & 7;
            const int morton = (inX & 1) | ((inY & 1) << 1) | ((inX & 2) << 1) | ((inY & 2) << 2) | ((inX & 4) << 2) | ((inY & 4) << 3);
            const size_t index = (size_t(tileY) * (width >> 3) + tileX) * 64 + morton;
            const uint8_t *p = rgba + (size_t(y) * width + x) * 4;
            dst[index] = (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
        }
    }
    C3D_TexFlush(&tex);
    C3D_TexSetFilter(&tex, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(&tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    sub.width = u16(width);
    sub.height = u16(height);
    sub.left = 0.0f;
    sub.top = 1.0f;
    sub.right = 1.0f;
    sub.bottom = 0.0f;
    return true;
}

bool loadFromBytes(const uint8_t *data, size_t size, Image &out)
{
    out.failed = true;
    if (size < 8) return false;
    std::vector<uint8_t> rgb;
    int width = 0;
    int height = 0;
    bool ok = false;
    if (data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') {
        ok = decodePng(data, size, rgb, width, height);
    } else if (data[0] == 0xFF && data[1] == 0xD8) {
        ok = decodeJpeg(data, size, rgb, width, height);
    }
    if (!ok) return false;
    std::vector<uint8_t> rgba;
    resample(rgb, width, height, rgba);
    rgb.clear();
    rgb.shrink_to_fit();
    if (!makeTexture(rgba.data(), Size, Size, out.tex, out.sub)) return false;
    pickColors(rgba, out.colorA, out.colorB);
    out.ready = true;
    out.failed = false;
    return true;
}

bool loadFromFile(const std::string &path, Image &out)
{
    std::vector<uint8_t> bytes;
    if (!platform::readWholeFile(path, bytes, 6 * 1024 * 1024)) {
        out.failed = true;
        return false;
    }
    return loadFromBytes(bytes.data(), bytes.size(), out);
}

namespace {

struct Request {
    std::string key;
    std::string path;
    bool embedded;
};

struct Decoded {
    std::string key;
    std::vector<uint8_t> rgba;
    uint32_t colorA = 0;
    uint32_t colorB = 0;
    bool ok = false;
};

LightLock loaderLock;
std::deque<Request> pendingRequests;
std::deque<Decoded> finishedDecodes;
Thread loaderThread = nullptr;
volatile bool loaderRunning = false;

bool decodeToRgba(const uint8_t *data, size_t size, std::vector<uint8_t> &rgba)
{
    if (size < 8) return false;
    std::vector<uint8_t> rgb;
    int width = 0;
    int height = 0;
    bool ok = false;
    if (data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') ok = decodePng(data, size, rgb, width, height);
    else if (data[0] == 0xFF && data[1] == 0xD8) ok = decodeJpeg(data, size, rgb, width, height);
    if (!ok) return false;
    resample(rgb, width, height, rgba);
    return true;
}

void loaderMain(void *)
{
    while (loaderRunning) {
        Request job;
        bool have = false;
        LightLock_Lock(&loaderLock);
        if (!pendingRequests.empty()) {
            job = pendingRequests.front();
            pendingRequests.pop_front();
            have = true;
        }
        LightLock_Unlock(&loaderLock);
        if (!have) {
            svcSleepThread(30 * 1000 * 1000);
            continue;
        }
        Decoded result;
        result.key = job.key;
        std::vector<uint8_t> bytes;
        if (job.embedded) {
            core::Picture picture;
            if (platform::readPictureFromFile(job.path, core::formatFromName(job.path), picture)) bytes = std::move(picture.data);
        } else {
            platform::readWholeFile(job.path, bytes, 6 * 1024 * 1024);
        }
        if (!bytes.empty() && decodeToRgba(bytes.data(), bytes.size(), result.rgba)) {
            pickColors(result.rgba, result.colorA, result.colorB);
            result.ok = true;
        }
        LightLock_Lock(&loaderLock);
        finishedDecodes.push_back(std::move(result));
        LightLock_Unlock(&loaderLock);
    }
}

} // namespace

void startLoader()
{
    LightLock_Init(&loaderLock);
    loaderRunning = true;
    s32 prio = 0;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    // Lower priority than the UI thread on the same core: it only runs while
    // the UI waits for the next frame.
    loaderThread = threadCreate(loaderMain, nullptr, 64 * 1024, prio + 1, -2, false);
}

void stopLoader()
{
    if (loaderThread == nullptr) return;
    loaderRunning = false;
    threadJoin(loaderThread, U64_MAX);
    threadFree(loaderThread);
    loaderThread = nullptr;
}

void request(const std::string &key, const std::string &path, bool embedded)
{
    LightLock_Lock(&loaderLock);
    pendingRequests.push_back({key, path, embedded});
    LightLock_Unlock(&loaderLock);
}

bool takeFinished(std::string &key, Image &out)
{
    Decoded result;
    LightLock_Lock(&loaderLock);
    const bool have = !finishedDecodes.empty();
    if (have) {
        result = std::move(finishedDecodes.front());
        finishedDecodes.pop_front();
    }
    LightLock_Unlock(&loaderLock);
    if (!have) return false;
    key = result.key;
    out.failed = !result.ok;
    out.ready = false;
    if (result.ok && makeTexture(result.rgba.data(), Size, Size, out.tex, out.sub)) {
        out.colorA = result.colorA;
        out.colorB = result.colorB;
        out.ready = true;
        out.failed = false;
    }
    return true;
}

void release(Image &image)
{
    if (image.ready) C3D_TexDelete(&image.tex);
    image.ready = false;
    image.failed = false;
}

void draw(const Image &image, float x, float y, float w, float h, float depth)
{
    if (!image.ready) return;
    C2D_Image img{const_cast<C3D_Tex *>(&image.tex), &image.sub};
    C2D_DrawImageAt(img, x, y, depth, nullptr, w / Size, h / Size);
}

} // namespace art
