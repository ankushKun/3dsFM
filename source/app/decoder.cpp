#include "decoder.hpp"

#include <mpg123.h>
#include <opus/opusfile.h>
#include <tremor/ivorbisfile.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace audio {

namespace {

// Every decoder reads the card through one of these: a stdio stream with a
// 64 KB buffer, so the SD card is asked for a large block a few times a
// second instead of a few hundred bytes a few hundred times a second. That
// is what keeps the music going while the scanner or the cover loader has
// the card busy. The decoders' own reads then come from memory.
constexpr size_t FileBufferSize = 64 * 1024;

struct BufferedFile {
    FILE *f = nullptr;
    explicit BufferedFile(const std::string &path)
    {
        f = std::fopen(path.c_str(), "rb");
        if (f != nullptr) std::setvbuf(f, nullptr, _IOFBF, FileBufferSize);
    }
    ~BufferedFile()
    {
        if (f != nullptr) std::fclose(f);
    }
    BufferedFile(const BufferedFile &) = delete;
    BufferedFile &operator=(const BufferedFile &) = delete;
};

// mpg123's reader hooks.
ssize_t bufferedRead(void *handle, void *buffer, size_t bytes)
{
    return ssize_t(std::fread(buffer, 1, bytes, static_cast<BufferedFile *>(handle)->f));
}
off_t bufferedSeek(void *handle, off_t offset, int whence)
{
    FILE *f = static_cast<BufferedFile *>(handle)->f;
    if (std::fseek(f, long(offset), whence) != 0) return -1;
    return off_t(std::ftell(f));
}

// Tremor's.
size_t vorbisRead(void *ptr, size_t size, size_t count, void *source)
{
    return std::fread(ptr, size, count, static_cast<BufferedFile *>(source)->f);
}
int vorbisSeek(void *source, ogg_int64_t offset, int whence)
{
    return std::fseek(static_cast<BufferedFile *>(source)->f, long(offset), whence);
}
long vorbisTell(void *source)
{
    return std::ftell(static_cast<BufferedFile *>(source)->f);
}

// opusfile's.
int opusRead(void *source, unsigned char *ptr, int bytes)
{
    return int(std::fread(ptr, 1, size_t(bytes), static_cast<BufferedFile *>(source)->f));
}
int opusSeek(void *source, opus_int64 offset, int whence)
{
    return std::fseek(static_cast<BufferedFile *>(source)->f, long(offset), whence);
}
opus_int64 opusTell(void *source)
{
    return std::ftell(static_cast<BufferedFile *>(source)->f);
}

class Mp3Decoder : public Decoder {
public:
    ~Mp3Decoder() override
    {
        if (handle_ != nullptr) {
            mpg123_close(handle_);
            mpg123_delete(handle_);
        }
        file_.reset();
    }

    bool open(const std::string &path, std::string &error) override
    {
        static bool initialised = false;
        if (!initialised) {
            mpg123_init();
            initialised = true;
        }
        int code = 0;
        handle_ = mpg123_new(nullptr, &code);
        if (handle_ == nullptr) {
            error = "mpg123 could not start.";
            return false;
        }
        // Keep the decoder's own buffers small; the console has little RAM.
        mpg123_param(handle_, MPG123_FLAGS, MPG123_GAPLESS | MPG123_QUIET, 0.0);
        file_.reset(new BufferedFile(path));
        if (file_->f == nullptr) {
            error = "Could not open the file.";
            return false;
        }
        mpg123_replace_reader_handle(handle_, bufferedRead, bufferedSeek, nullptr);
        if (mpg123_open_handle(handle_, file_.get()) != MPG123_OK) {
            error = "Could not open the MP3.";
            return false;
        }
        long rate = 0;
        int channels = 0;
        int encoding = 0;
        if (mpg123_getformat(handle_, &rate, &channels, &encoding) != MPG123_OK) {
            error = "Unreadable MP3 stream.";
            return false;
        }
        // Lock the output to stereo 16-bit at the stream's rate.
        mpg123_format_none(handle_);
        mpg123_format(handle_, rate, MPG123_STEREO, MPG123_ENC_SIGNED_16);
        rate_ = int(rate);
        return true;
    }

    int sampleRate() const override { return rate_; }

    size_t read(int16_t *out, size_t frames) override
    {
        size_t done = 0;
        const int code = mpg123_read(handle_, reinterpret_cast<unsigned char *>(out), frames * 4, &done);
        if (code != MPG123_OK && code != MPG123_NEW_FORMAT && code != MPG123_DONE) return 0;
        if (code == MPG123_DONE && done == 0) return 0;
        return done / 4;
    }

    bool seek(double seconds) override
    {
        return mpg123_seek(handle_, off_t(seconds * rate_), SEEK_SET) >= 0;
    }

    double position() const override
    {
        return rate_ > 0 ? double(mpg123_tell(handle_)) / rate_ : 0.0;
    }

private:
    mpg123_handle *handle_ = nullptr;
    std::unique_ptr<BufferedFile> file_;
    int rate_ = 44100;
};

class VorbisDecoder : public Decoder {
public:
    ~VorbisDecoder() override
    {
        if (opened_) ov_clear(&file_);
        source_.reset();
    }

    bool open(const std::string &path, std::string &error) override
    {
        source_.reset(new BufferedFile(path));
        if (source_->f == nullptr) {
            error = "Could not open the file.";
            return false;
        }
        const ov_callbacks callbacks = {vorbisRead, vorbisSeek, nullptr, vorbisTell};
        if (ov_open_callbacks(source_.get(), &file_, nullptr, 0, callbacks) < 0) {
            error = "Not an Ogg Vorbis file.";
            return false;
        }
        opened_ = true;
        const vorbis_info *info = ov_info(&file_, -1);
        rate_ = info != nullptr ? int(info->rate) : 44100;
        channels_ = info != nullptr ? info->channels : 2;
        return true;
    }

    int sampleRate() const override { return rate_; }

    size_t read(int16_t *out, size_t frames) override
    {
        if (channels_ == 2) {
            size_t got = 0;
            while (got < frames) {
                int bitstream = 0;
                const long bytes = ov_read(&file_, reinterpret_cast<char *>(out + got * 2), int((frames - got) * 4), &bitstream);
                if (bytes <= 0) break;
                got += size_t(bytes) / 4;
            }
            return got;
        }
        // Mono (or more): decode into a scratch buffer and spread to stereo.
        scratch_.resize(frames * channels_);
        size_t got = 0;
        while (got < frames) {
            int bitstream = 0;
            const long bytes = ov_read(&file_, reinterpret_cast<char *>(scratch_.data() + got * channels_), int((frames - got) * channels_ * 2), &bitstream);
            if (bytes <= 0) break;
            got += size_t(bytes) / (channels_ * 2);
        }
        for (size_t i = 0; i < got; ++i) {
            out[i * 2] = scratch_[i * channels_];
            out[i * 2 + 1] = scratch_[i * channels_ + (channels_ > 1 ? 1 : 0)];
        }
        return got;
    }

    bool seek(double seconds) override { return ov_time_seek(&file_, ogg_int64_t(seconds * 1000.0)) == 0; }

    double position() const override { return double(ov_time_tell(const_cast<OggVorbis_File *>(&file_))) / 1000.0; }

private:
    OggVorbis_File file_{};
    std::unique_ptr<BufferedFile> source_;
    bool opened_ = false;
    int rate_ = 44100;
    int channels_ = 2;
    std::vector<int16_t> scratch_;
};

class OpusDecoder : public Decoder {
public:
    ~OpusDecoder() override
    {
        if (file_ != nullptr) op_free(file_);
        source_.reset();
    }

    bool open(const std::string &path, std::string &error) override
    {
        source_.reset(new BufferedFile(path));
        if (source_->f == nullptr) {
            error = "Could not open the file.";
            return false;
        }
        int code = 0;
        const OpusFileCallbacks callbacks = {opusRead, opusSeek, opusTell, nullptr};
        file_ = op_open_callbacks(source_.get(), &callbacks, nullptr, 0, &code);
        if (file_ == nullptr) {
            error = "Not an Opus file.";
            return false;
        }
        return true;
    }

    int sampleRate() const override { return 48000; }

    size_t read(int16_t *out, size_t frames) override
    {
        size_t got = 0;
        while (got < frames) {
            const int samples = op_read_stereo(file_, out + got * 2, int((frames - got) * 2));
            if (samples <= 0) break;
            got += size_t(samples);
        }
        return got;
    }

    bool seek(double seconds) override { return op_pcm_seek(file_, ogg_int64_t(seconds * 48000.0)) == 0; }

    double position() const override { return double(op_pcm_tell(file_)) / 48000.0; }

private:
    OggOpusFile *file_ = nullptr;
    std::unique_ptr<BufferedFile> source_;
};

class WavDecoder : public Decoder {
public:
    ~WavDecoder() override
    {
        if (file_ != nullptr) std::fclose(file_);
    }

    bool open(const std::string &path, std::string &error) override
    {
        file_ = std::fopen(path.c_str(), "rb");
        if (file_ == nullptr) {
            error = "Could not open the file.";
            return false;
        }
        std::setvbuf(file_, nullptr, _IOFBF, FileBufferSize);
        uint8_t riff[12];
        if (std::fread(riff, 1, 12, file_) != 12 || std::memcmp(riff, "RIFF", 4) != 0 || std::memcmp(riff + 8, "WAVE", 4) != 0) {
            error = "Not a WAV file.";
            return false;
        }
        for (;;) {
            uint8_t ch[8];
            if (std::fread(ch, 1, 8, file_) != 8) {
                error = "No audio data in the WAV.";
                return false;
            }
            const uint32_t len = uint32_t(ch[4]) | (uint32_t(ch[5]) << 8) | (uint32_t(ch[6]) << 16) | (uint32_t(ch[7]) << 24);
            if (std::memcmp(ch, "fmt ", 4) == 0) {
                uint8_t fmt[16];
                if (len < 16 || std::fread(fmt, 1, 16, file_) != 16) {
                    error = "Bad WAV format chunk.";
                    return false;
                }
                const int format = fmt[0] | (fmt[1] << 8);
                channels_ = fmt[2] | (fmt[3] << 8);
                rate_ = int(uint32_t(fmt[4]) | (uint32_t(fmt[5]) << 8) | (uint32_t(fmt[6]) << 16) | (uint32_t(fmt[7]) << 24));
                bits_ = fmt[14] | (fmt[15] << 8);
                if (format != 1 || (bits_ != 16 && bits_ != 8) || channels_ < 1 || channels_ > 2) {
                    error = "Only 8 or 16-bit PCM WAV is supported.";
                    return false;
                }
                std::fseek(file_, long(len - 16 + (len & 1)), SEEK_CUR);
            } else if (std::memcmp(ch, "data", 4) == 0) {
                dataStart_ = std::ftell(file_);
                dataBytes_ = len;
                return rate_ > 0;
            } else {
                std::fseek(file_, long(len + (len & 1)), SEEK_CUR);
            }
        }
    }

    int sampleRate() const override { return rate_; }

    size_t read(int16_t *out, size_t frames) override
    {
        const size_t frameBytes = size_t(channels_ * bits_ / 8);
        const size_t remaining = (dataBytes_ - consumed_) / frameBytes;
        const size_t want = std::min(frames, remaining);
        if (want == 0) return 0;
        scratch_.resize(want * frameBytes);
        const size_t got = std::fread(scratch_.data(), frameBytes, want, file_);
        consumed_ += got * frameBytes;
        for (size_t i = 0; i < got; ++i) {
            int16_t left;
            int16_t right;
            if (bits_ == 16) {
                const uint8_t *p = scratch_.data() + i * frameBytes;
                left = int16_t(p[0] | (p[1] << 8));
                right = channels_ == 2 ? int16_t(p[2] | (p[3] << 8)) : left;
            } else {
                const uint8_t *p = scratch_.data() + i * frameBytes;
                left = int16_t((int(p[0]) - 128) << 8);
                right = channels_ == 2 ? int16_t((int(p[1]) - 128) << 8) : left;
            }
            out[i * 2] = left;
            out[i * 2 + 1] = right;
        }
        return got;
    }

    bool seek(double seconds) override
    {
        const size_t frameBytes = size_t(channels_ * bits_ / 8);
        size_t target = size_t(seconds * rate_) * frameBytes;
        if (target > dataBytes_) target = dataBytes_;
        consumed_ = target;
        return std::fseek(file_, dataStart_ + long(target), SEEK_SET) == 0;
    }

    double position() const override
    {
        const size_t frameBytes = size_t(channels_ * bits_ / 8);
        return rate_ > 0 ? double(consumed_ / frameBytes) / rate_ : 0.0;
    }

private:
    FILE *file_ = nullptr;
    int rate_ = 0;
    int channels_ = 2;
    int bits_ = 16;
    long dataStart_ = 0;
    size_t dataBytes_ = 0;
    size_t consumed_ = 0;
    std::vector<uint8_t> scratch_;
};

} // namespace

std::unique_ptr<Decoder> makeDecoder(core::Format format)
{
    switch (format) {
    case core::Format::Mp3: return std::make_unique<Mp3Decoder>();
    case core::Format::OggVorbis: return std::make_unique<VorbisDecoder>();
    case core::Format::Opus: return std::make_unique<OpusDecoder>();
    case core::Format::Wav: return std::make_unique<WavDecoder>();
    default: return nullptr;
    }
}

} // namespace audio
