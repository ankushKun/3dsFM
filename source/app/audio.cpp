#include "audio.hpp"

#include "decoder.hpp"
#include "platform.hpp"

#include <3ds.h>

#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>

namespace audio {

namespace {

constexpr int Channel = 0;
constexpr int BufferCount = 4;
constexpr int FramesPerBuffer = 4096;
constexpr int RecentSamples = 1024;
constexpr size_t DecodeStackSize = 192 * 1024;

bool dspReady = false;
bool loggedCore = false;

} // namespace

bool init(std::string &error)
{
    if (R_FAILED(ndspInit())) {
        error = "No DSP firmware. Dump it with DSP1 (sdmc:/3ds/dspfirm.cdc).";
        return false;
    }
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    dspReady = true;
    return true;
}

void shutdown()
{
    if (dspReady) {
        ndspExit();
        dspReady = false;
    }
}

bool ready()
{
    return dspReady;
}

struct Player::Impl {
    ndspWaveBuf bufs[BufferCount]{};
    int16_t *memory = nullptr;
    std::unique_ptr<Decoder> decoder;
    Thread thread = nullptr;
    LightEvent wake{};
    std::atomic<bool> running{false};
    std::atomic<bool> paused{false};
    std::atomic<bool> ended{false};
    std::atomic<bool> finishedFlag{false};
    std::atomic<double> decodedSeconds{0.0};
    std::atomic<float> peak{0.0f};
    float recent[RecentSamples]{};
    std::atomic<int> recentHead{0};
    int rate = 48000;
    bool loaded = false;

    static void entry(void *arg) { static_cast<Impl *>(arg)->loop(); }

    void loop()
    {
        while (running) {
            bool fed = false;
            if (!paused && !ended) {
                for (int i = 0; i < BufferCount; ++i) {
                    if (bufs[i].status == NDSP_WBUF_DONE || bufs[i].status == NDSP_WBUF_FREE) {
                        if (!fill(i)) {
                            ended = true;
                            break;
                        }
                        fed = true;
                    }
                }
            }
            if (ended && !finishedFlag) {
                // Wait for the last buffers to drain before reporting the end.
                bool anyPlaying = false;
                for (int i = 0; i < BufferCount; ++i) {
                    if (bufs[i].status == NDSP_WBUF_QUEUED || bufs[i].status == NDSP_WBUF_PLAYING) anyPlaying = true;
                }
                if (!anyPlaying) finishedFlag = true;
            }
            if (!fed) svcSleepThread(8 * 1000 * 1000);
        }
    }

    bool fill(int which)
    {
        int16_t *out = static_cast<int16_t *>(const_cast<void *>(bufs[which].data_vaddr));
        const size_t got = decoder->read(out, FramesPerBuffer);
        if (got == 0) return false;
        // Keep a mono copy of the tail for the visualizer and the level meter.
        float maxAbs = 0.0f;
        int head = recentHead.load();
        const size_t step = got > RecentSamples ? got / RecentSamples : 1;
        for (size_t i = 0; i < got; i += step) {
            const float v = (float(out[i * 2]) + float(out[i * 2 + 1])) / 65536.0f;
            recent[head] = v;
            head = (head + 1) % RecentSamples;
            const float a = std::fabs(v);
            if (a > maxAbs) maxAbs = a;
        }
        recentHead = head;
        peak = maxAbs;
        bufs[which].nsamples = got;
        DSP_FlushDataCache(bufs[which].data_vaddr, got * 4);
        ndspChnWaveBufAdd(Channel, &bufs[which]);
        decodedSeconds = decoder->position();
        return true;
    }

    void startThread()
    {
        running = true;
        // Decoding runs on the second core when the system lets us have it
        // (New 3DS, or an Old 3DS with the app CPU limit set), else core 0.
        // libopus and mpg123 keep sizeable working sets on the stack, so the
        // thread gets far more than the 32 KB it once had.
        s32 prio = 0;
        svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
        thread = threadCreate(entry, this, DecodeStackSize, prio - 1, 1, false);
        if (thread == nullptr) {
            thread = threadCreate(entry, this, DecodeStackSize, prio - 1, -2, false);
            if (!loggedCore) platform::log("audio: decoding on the app core");
        } else if (!loggedCore) {
            platform::log("audio: decoding on the second core");
        }
        loggedCore = true;
    }

    void stopThread()
    {
        if (thread == nullptr) return;
        running = false;
        threadJoin(thread, U64_MAX);
        threadFree(thread);
        thread = nullptr;
    }
};

Player::Player() : impl_(new Impl)
{
    const size_t bytes = sizeof(int16_t) * 2 * FramesPerBuffer * BufferCount;
    impl_->memory = static_cast<int16_t *>(linearAlloc(bytes));
    if (impl_->memory != nullptr) std::memset(impl_->memory, 0, bytes);
    for (int i = 0; i < BufferCount; ++i) {
        impl_->bufs[i].data_vaddr = impl_->memory + i * FramesPerBuffer * 2;
        impl_->bufs[i].nsamples = FramesPerBuffer;
        impl_->bufs[i].status = NDSP_WBUF_FREE;
    }
}

Player::~Player()
{
    stop();
    if (impl_->memory != nullptr) linearFree(impl_->memory);
    delete impl_;
}

bool Player::play(const std::string &path, core::Format format, std::string &error)
{
    stop();
    if (!dspReady) {
        error = "Audio is not available.";
        return false;
    }
    impl_->decoder = makeDecoder(format);
    if (!impl_->decoder) {
        error = "This format is not supported.";
        return false;
    }
    if (!impl_->decoder->open(path, error)) {
        impl_->decoder.reset();
        return false;
    }
    impl_->rate = impl_->decoder->sampleRate();
    ndspChnReset(Channel);
    ndspChnSetInterp(Channel, NDSP_INTERP_LINEAR);
    ndspChnSetRate(Channel, float(impl_->rate));
    ndspChnSetFormat(Channel, NDSP_FORMAT_STEREO_PCM16);
    float mix[12] = {};
    mix[0] = 1.0f;
    mix[1] = 1.0f;
    ndspChnSetMix(Channel, mix);
    for (int i = 0; i < BufferCount; ++i) impl_->bufs[i].status = NDSP_WBUF_FREE;
    impl_->paused = false;
    impl_->ended = false;
    impl_->finishedFlag = false;
    impl_->decodedSeconds = 0.0;
    impl_->loaded = true;
    impl_->startThread();
    return true;
}

void Player::stop()
{
    impl_->stopThread();
    if (dspReady) ndspChnReset(Channel);
    impl_->decoder.reset();
    impl_->loaded = false;
    impl_->paused = false;
    impl_->peak = 0.0f;
}

void Player::pause()
{
    if (!impl_->loaded) return;
    impl_->paused = true;
    ndspChnSetPaused(Channel, true);
}

void Player::resume()
{
    if (!impl_->loaded) return;
    impl_->paused = false;
    ndspChnSetPaused(Channel, false);
}

void Player::toggle()
{
    if (impl_->paused) resume(); else pause();
}

bool Player::playing() const
{
    return impl_->loaded && !impl_->paused && !impl_->finishedFlag;
}

bool Player::loaded() const
{
    return impl_->loaded;
}

bool Player::finished()
{
    if (impl_->finishedFlag) {
        impl_->finishedFlag = false;
        impl_->ended = false;
        impl_->loaded = false;
        impl_->stopThread();
        return true;
    }
    return false;
}

double Player::positionSeconds() const
{
    if (!impl_->loaded) return 0.0;
    // Decoded time minus whatever is still queued ahead of the DSP.
    int queued = 0;
    for (int i = 0; i < BufferCount; ++i) {
        if (impl_->bufs[i].status == NDSP_WBUF_QUEUED) queued += int(impl_->bufs[i].nsamples);
    }
    const double ahead = impl_->rate > 0 ? double(queued) / impl_->rate : 0.0;
    const double p = impl_->decodedSeconds - ahead;
    return p < 0.0 ? 0.0 : p;
}

void Player::seek(double seconds)
{
    if (!impl_->loaded || !impl_->decoder) return;
    const bool wasPaused = impl_->paused;
    impl_->paused = true;
    svcSleepThread(10 * 1000 * 1000);
    ndspChnWaveBufClear(Channel);
    for (int i = 0; i < BufferCount; ++i) impl_->bufs[i].status = NDSP_WBUF_FREE;
    impl_->decoder->seek(seconds < 0.0 ? 0.0 : seconds);
    impl_->decodedSeconds = impl_->decoder->position();
    impl_->ended = false;
    impl_->paused = wasPaused;
}

int Player::recentSamples(float *out, int count) const
{
    const int n = count < RecentSamples ? count : RecentSamples;
    int head = impl_->recentHead.load();
    for (int i = 0; i < n; ++i) {
        out[i] = impl_->recent[(head + RecentSamples - n + i) % RecentSamples];
    }
    return n;
}

float Player::level() const
{
    return playing() ? impl_->peak.load() : 0.0f;
}

} // namespace audio
