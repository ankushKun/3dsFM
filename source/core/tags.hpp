#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

// Metadata readers for the formats 3dsFM plays. Portable: they read through
// a ByteSource so the host tests can feed them synthetic files.
namespace core {

enum class Format { Unknown, Mp3, OggVorbis, Opus, Wav };

Format formatFromName(const std::string &fileName);
const char *formatName(Format format);

struct ByteSource {
    virtual ~ByteSource() = default;
    // Reads up to n bytes at offset; returns how many were read.
    virtual size_t read(uint64_t offset, void *buffer, size_t n) = 0;
    virtual uint64_t size() = 0;
};

struct MemorySource : ByteSource {
    std::vector<uint8_t> bytes;
    explicit MemorySource(std::vector<uint8_t> b) : bytes(std::move(b)) {}
    size_t read(uint64_t offset, void *buffer, size_t n) override;
    uint64_t size() override { return bytes.size(); }
};

struct Picture {
    std::string mime;
    std::vector<uint8_t> data;
    bool empty() const { return data.empty(); }
};

struct Tags {
    std::string title;
    std::string artist;
    std::string album;
    std::string albumArtist;
    std::string year;
    int track = 0;
    int durationSeconds = 0;
    int sampleRate = 0;
    int channels = 0;
    Picture picture;
};

// Fills tags from the file. Returns false only when the file is not the
// format it claims to be; missing tags are not an error.
bool readTags(ByteSource &source, Format format, Tags &out);

// Only the embedded picture, for lazy art loading.
bool readPicture(ByteSource &source, Format format, Picture &out);

std::vector<uint8_t> base64Decode(const std::string &text);

} // namespace core
