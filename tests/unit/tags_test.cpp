#include "../check.hpp"
#include "tags.hpp"

#include <cstring>
#include <string>
#include <vector>

using namespace core;

namespace {

void put32be(std::vector<uint8_t> &v, uint32_t x) { v.push_back(x >> 24); v.push_back(x >> 16); v.push_back(x >> 8); v.push_back(x); }
void put32le(std::vector<uint8_t> &v, uint32_t x) { v.push_back(x); v.push_back(x >> 8); v.push_back(x >> 16); v.push_back(x >> 24); }
void putSyncsafe(std::vector<uint8_t> &v, uint32_t x) { v.push_back((x >> 21) & 0x7F); v.push_back((x >> 14) & 0x7F); v.push_back((x >> 7) & 0x7F); v.push_back(x & 0x7F); }
void putStr(std::vector<uint8_t> &v, const std::string &s) { v.insert(v.end(), s.begin(), s.end()); }

std::vector<uint8_t> textFrame(const std::string &id, const std::string &text, int encoding = 3, int version = 3)
{
    std::vector<uint8_t> body;
    body.push_back(uint8_t(encoding));
    if (encoding == 1) {
        body.push_back(0xFF);
        body.push_back(0xFE);
        for (char c : text) { body.push_back(uint8_t(c)); body.push_back(0); }
    } else {
        putStr(body, text);
    }
    std::vector<uint8_t> frame;
    putStr(frame, id);
    if (version == 4) putSyncsafe(frame, body.size()); else put32be(frame, body.size());
    frame.push_back(0);
    frame.push_back(0);
    frame.insert(frame.end(), body.begin(), body.end());
    return frame;
}

std::vector<uint8_t> apicFrame(const std::string &mime, const std::vector<uint8_t> &data)
{
    std::vector<uint8_t> body{0};
    putStr(body, mime);
    body.push_back(0);
    body.push_back(3);
    body.push_back(0);
    body.insert(body.end(), data.begin(), data.end());
    std::vector<uint8_t> frame;
    putStr(frame, "APIC");
    put32be(frame, body.size());
    frame.push_back(0);
    frame.push_back(0);
    frame.insert(frame.end(), body.begin(), body.end());
    return frame;
}

std::vector<uint8_t> id3(const std::vector<std::vector<uint8_t>> &frames, int version = 3)
{
    std::vector<uint8_t> body;
    for (const auto &f : frames) body.insert(body.end(), f.begin(), f.end());
    std::vector<uint8_t> tag{'I', 'D', '3', uint8_t(version), 0, 0};
    putSyncsafe(tag, body.size());
    tag.insert(tag.end(), body.begin(), body.end());
    return tag;
}

// One MPEG-1 Layer III frame header, 128 kbps, 44.1 kHz, stereo, plus junk.
std::vector<uint8_t> mp3Frames(int frames)
{
    std::vector<uint8_t> out;
    for (int i = 0; i < frames; ++i) {
        out.push_back(0xFF); out.push_back(0xFB); out.push_back(0x90); out.push_back(0x00);
        out.resize(out.size() + 417 - 4, 0);
    }
    return out;
}

std::vector<uint8_t> oggPage(const std::vector<std::vector<uint8_t>> &packets, uint64_t granule, int sequence)
{
    std::vector<uint8_t> page;
    putStr(page, "OggS");
    page.push_back(0);
    page.push_back(sequence == 0 ? 2 : 0);
    for (int i = 0; i < 8; ++i) page.push_back(uint8_t(granule >> (8 * i)));
    put32le(page, 1);
    put32le(page, sequence);
    put32le(page, 0);
    std::vector<uint8_t> table;
    for (const auto &p : packets) {
        size_t n = p.size();
        while (n >= 255) { table.push_back(255); n -= 255; }
        table.push_back(uint8_t(n));
    }
    page.push_back(uint8_t(table.size()));
    page.insert(page.end(), table.begin(), table.end());
    for (const auto &p : packets) page.insert(page.end(), p.begin(), p.end());
    return page;
}

std::vector<uint8_t> vorbisComment(const std::string &magic, const std::vector<std::string> &entries)
{
    std::vector<uint8_t> c;
    putStr(c, magic);
    put32le(c, 4);
    putStr(c, "test");
    put32le(c, entries.size());
    for (const auto &e : entries) { put32le(c, e.size()); putStr(c, e); }
    if (magic[0] == 3) c.push_back(1);
    return c;
}

} // namespace

TEST("ID3v2.3 text frames, track number and duration from a CBR stream")
{
    std::vector<uint8_t> file = id3({textFrame("TIT2", "Stay With Me"), textFrame("TPE1", "Miki Matsubara", 1), textFrame("TALB", "Pocket Park", 0), textFrame("TYER", "1980"), textFrame("TRCK", "2/9")});
    const std::vector<uint8_t> audio = mp3Frames(100);
    file.insert(file.end(), audio.begin(), audio.end());
    MemorySource source(file);
    Tags tags;
    CHECK(readTags(source, Format::Mp3, tags));
    CHECK_EQ(tags.title, std::string("Stay With Me"));
    CHECK_EQ(tags.artist, std::string("Miki Matsubara"));
    CHECK_EQ(tags.album, std::string("Pocket Park"));
    CHECK_EQ(tags.year, std::string("1980"));
    CHECK_EQ(tags.track, 2);
    CHECK_EQ(tags.sampleRate, 44100);
    CHECK_EQ(tags.channels, 2);
    // 100 frames * 417 bytes * 8 bits / 128000 = 2.6 s
    CHECK_EQ(tags.durationSeconds, 2);
}

TEST("ID3v2.4 syncsafe frame sizes and embedded front cover")
{
    const std::vector<uint8_t> png{0x89, 'P', 'N', 'G', 1, 2, 3};
    std::vector<uint8_t> file = id3({textFrame("TIT2", "Plastic Love", 3, 4), textFrame("TDRC", "1984-04-25", 3, 4), apicFrame("image/png", png)}, 4);
    const std::vector<uint8_t> audio = mp3Frames(10);
    file.insert(file.end(), audio.begin(), audio.end());
    MemorySource source(file);
    Tags tags;
    CHECK(readTags(source, Format::Mp3, tags));
    CHECK_EQ(tags.title, std::string("Plastic Love"));
    CHECK_EQ(tags.year, std::string("1984"));
    CHECK_EQ(tags.picture.mime, std::string("image/png"));
    CHECK(tags.picture.data == png);
    Picture only;
    CHECK(readPicture(source, Format::Mp3, only));
    CHECK(only.data == png);
}

TEST("ID3v1 fills what ID3v2 left empty")
{
    std::vector<uint8_t> file = mp3Frames(5);
    std::vector<uint8_t> v1(128, 0);
    std::memcpy(v1.data(), "TAG", 3);
    std::memcpy(v1.data() + 3, "Old Song", 8);
    std::memcpy(v1.data() + 33, "Old Band", 8);
    v1[126] = 7;
    file.insert(file.end(), v1.begin(), v1.end());
    MemorySource source(file);
    Tags tags;
    CHECK(readTags(source, Format::Mp3, tags));
    CHECK_EQ(tags.title, std::string("Old Song"));
    CHECK_EQ(tags.artist, std::string("Old Band"));
    CHECK_EQ(tags.track, 7);
}

TEST("Ogg Vorbis comments and duration from the last granule")
{
    std::vector<uint8_t> ident;
    putStr(ident, std::string("\x01vorbis", 7));
    put32le(ident, 0);
    ident.push_back(2);
    put32le(ident, 44100);
    ident.resize(30, 0);
    std::vector<uint8_t> file = oggPage({ident}, 0, 0);
    const auto page2 = oggPage({vorbisComment(std::string("\x03vorbis", 7), {"TITLE=Midnight Pretenders", "ARTIST=Tomoko Aran", "ALBUM=Fuyu-Kukan", "DATE=1983", "TRACKNUMBER=1"})}, 0, 1);
    file.insert(file.end(), page2.begin(), page2.end());
    const auto page3 = oggPage({std::vector<uint8_t>(100, 0)}, 44100 * 301, 2);
    file.insert(file.end(), page3.begin(), page3.end());
    MemorySource source(file);
    Tags tags;
    CHECK(readTags(source, Format::OggVorbis, tags));
    CHECK_EQ(tags.title, std::string("Midnight Pretenders"));
    CHECK_EQ(tags.artist, std::string("Tomoko Aran"));
    CHECK_EQ(tags.album, std::string("Fuyu-Kukan"));
    CHECK_EQ(tags.year, std::string("1983"));
    CHECK_EQ(tags.track, 1);
    CHECK_EQ(tags.sampleRate, 44100);
    CHECK_EQ(tags.durationSeconds, 301);
}

TEST("Opus tags, pre-skip and a base64 picture block")
{
    std::vector<uint8_t> head;
    putStr(head, "OpusHead");
    head.push_back(1);
    head.push_back(2);
    head.push_back(0x38); head.push_back(0x01); // pre-skip 312
    put32le(head, 48000);
    head.push_back(0); head.push_back(0); head.push_back(0);
    // FLAC picture block around a 3-byte "image".
    std::vector<uint8_t> block;
    put32be(block, 3);
    put32be(block, 10); putStr(block, "image/jpeg");
    put32be(block, 0);
    for (int i = 0; i < 4; ++i) put32be(block, 0);
    put32be(block, 3); block.push_back(0xFF); block.push_back(0xD8); block.push_back(0xFF);
    static const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string b64;
    for (size_t i = 0; i < block.size(); i += 3) {
        uint32_t v = block[i] << 16;
        if (i + 1 < block.size()) v |= block[i + 1] << 8;
        if (i + 2 < block.size()) v |= block[i + 2];
        b64 += alphabet[(v >> 18) & 63];
        b64 += alphabet[(v >> 12) & 63];
        b64 += i + 1 < block.size() ? alphabet[(v >> 6) & 63] : '=';
        b64 += i + 2 < block.size() ? alphabet[v & 63] : '=';
    }
    std::vector<uint8_t> file = oggPage({head}, 0, 0);
    const auto page2 = oggPage({vorbisComment("OpusTags", {"title=Fly-Day Chinatown", "ALBUMARTIST=Yasuha", "METADATA_BLOCK_PICTURE=" + b64})}, 0, 1);
    file.insert(file.end(), page2.begin(), page2.end());
    const auto page3 = oggPage({std::vector<uint8_t>(10, 0)}, 48000 * 275 + 312, 2);
    file.insert(file.end(), page3.begin(), page3.end());
    MemorySource source(file);
    Tags tags;
    CHECK(readTags(source, Format::Opus, tags));
    CHECK_EQ(tags.title, std::string("Fly-Day Chinatown"));
    CHECK_EQ(tags.albumArtist, std::string("Yasuha"));
    CHECK_EQ(tags.durationSeconds, 275);
    CHECK_EQ(tags.picture.mime, std::string("image/jpeg"));
    CHECK_EQ(tags.picture.data.size(), size_t(3));
}

TEST("WAV header gives rate, channels and duration")
{
    std::vector<uint8_t> file;
    putStr(file, "RIFF"); put32le(file, 0); putStr(file, "WAVE");
    putStr(file, "fmt "); put32le(file, 16);
    file.push_back(1); file.push_back(0); file.push_back(2); file.push_back(0);
    put32le(file, 22050); put32le(file, 22050 * 4);
    file.push_back(4); file.push_back(0); file.push_back(16); file.push_back(0);
    putStr(file, "data"); put32le(file, 22050 * 4 * 12);
    MemorySource source(file);
    Tags tags;
    CHECK(readTags(source, Format::Wav, tags));
    CHECK_EQ(tags.sampleRate, 22050);
    CHECK_EQ(tags.channels, 2);
    CHECK_EQ(tags.durationSeconds, 12);
}

TEST("formats come from the extension, case-insensitively")
{
    CHECK(formatFromName("song.MP3") == Format::Mp3);
    CHECK(formatFromName("song.ogg") == Format::OggVorbis);
    CHECK(formatFromName("song.opus") == Format::Opus);
    CHECK(formatFromName("song.wav") == Format::Wav);
    CHECK(formatFromName("cover.jpg") == Format::Unknown);
    CHECK(formatFromName("noext") == Format::Unknown);
}

TEST("a file that is not what its name says is rejected")
{
    MemorySource source(std::vector<uint8_t>(64, 0x42));
    Tags tags;
    CHECK(!readTags(source, Format::OggVorbis, tags));
    CHECK(!readTags(source, Format::Wav, tags));
}
