#include "tags.hpp"

#include <algorithm>
#include <cstring>

namespace core {

namespace {

std::string lower(std::string s)
{
    for (char &c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string trimmed(const std::string &s)
{
    size_t a = 0;
    size_t b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\0' || s[a] == '\n' || s[a] == '\r' || s[a] == '\t')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\0' || s[b - 1] == '\n' || s[b - 1] == '\r' || s[b - 1] == '\t')) --b;
    return s.substr(a, b - a);
}

uint32_t be32(const uint8_t *p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }
uint32_t le32(const uint8_t *p) { return (uint32_t(p[3]) << 24) | (uint32_t(p[2]) << 16) | (uint32_t(p[1]) << 8) | p[0]; }
uint16_t le16(const uint8_t *p) { return uint16_t((uint16_t(p[1]) << 8) | p[0]); }
uint64_t le64(const uint8_t *p) { return (uint64_t(le32(p + 4)) << 32) | le32(p); }
uint32_t syncsafe(const uint8_t *p) { return (uint32_t(p[0] & 0x7F) << 21) | (uint32_t(p[1] & 0x7F) << 14) | (uint32_t(p[2] & 0x7F) << 7) | (p[3] & 0x7F); }

std::vector<uint8_t> readAll(ByteSource &source, uint64_t offset, size_t n)
{
    std::vector<uint8_t> out(n);
    const size_t got = n == 0 ? 0 : source.read(offset, out.data(), n);
    out.resize(got);
    return out;
}

void appendUtf8(std::string &out, uint32_t cp)
{
    if (cp < 0x80) {
        out += char(cp);
    } else if (cp < 0x800) {
        out += char(0xC0 | (cp >> 6));
        out += char(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += char(0xE0 | (cp >> 12));
        out += char(0x80 | ((cp >> 6) & 0x3F));
        out += char(0x80 | (cp & 0x3F));
    } else {
        out += char(0xF0 | (cp >> 18));
        out += char(0x80 | ((cp >> 12) & 0x3F));
        out += char(0x80 | ((cp >> 6) & 0x3F));
        out += char(0x80 | (cp & 0x3F));
    }
}

// Decodes an ID3v2 text field. encoding: 0 Latin-1, 1 UTF-16 with BOM,
// 2 UTF-16BE, 3 UTF-8. Stops at the terminator and reports where it ended.
std::string decodeText(const uint8_t *data, size_t n, int encoding, size_t *consumed = nullptr)
{
    std::string out;
    size_t i = 0;
    if (encoding == 0) {
        for (; i < n && data[i] != 0; ++i) appendUtf8(out, data[i]);
        if (i < n) ++i;
    } else if (encoding == 3) {
        for (; i < n && data[i] != 0; ++i) out += char(data[i]);
        if (i < n) ++i;
    } else {
        bool bigEndian = encoding == 2;
        if (encoding == 1 && n >= 2) {
            if (data[0] == 0xFF && data[1] == 0xFE) { i = 2; bigEndian = false; }
            else if (data[0] == 0xFE && data[1] == 0xFF) { i = 2; bigEndian = true; }
        }
        uint32_t high = 0;
        for (; i + 1 < n; i += 2) {
            const uint16_t unit = bigEndian ? uint16_t((data[i] << 8) | data[i + 1]) : uint16_t((data[i + 1] << 8) | data[i]);
            if (unit == 0) { i += 2; break; }
            if (unit >= 0xD800 && unit < 0xDC00) { high = unit; continue; }
            if (unit >= 0xDC00 && unit < 0xE000 && high != 0) {
                appendUtf8(out, 0x10000 + ((high - 0xD800) << 10) + (unit - 0xDC00));
                high = 0;
                continue;
            }
            appendUtf8(out, unit);
        }
    }
    if (consumed != nullptr) *consumed = std::min(i, n);
    return trimmed(out);
}

std::vector<uint8_t> deunsync(const uint8_t *data, size_t n)
{
    std::vector<uint8_t> out;
    out.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        out.push_back(data[i]);
        if (data[i] == 0xFF && i + 1 < n && data[i + 1] == 0x00) ++i;
    }
    return out;
}

int parseLeadingInt(const std::string &s)
{
    int value = 0;
    bool any = false;
    for (char c : s) {
        if (c < '0' || c > '9') break;
        value = value * 10 + (c - '0');
        any = true;
        if (value > 100000) break;
    }
    return any ? value : 0;
}

// Returns the size of the ID3v2 tag at the start of the file (0 if none) and
// fills tags from its frames.
uint32_t readId3v2(ByteSource &source, Tags &tags, bool wantPicture, bool wantText)
{
    uint8_t header[10];
    if (source.read(0, header, 10) != 10 || std::memcmp(header, "ID3", 3) != 0) return 0;
    const int major = header[3];
    const uint8_t flags = header[5];
    const uint32_t size = syncsafe(header + 6);
    const uint32_t total = 10 + size + ((flags & 0x10) ? 10 : 0);
    if (major < 3 || major > 4 || size == 0) return total;

    constexpr uint32_t MaxTag = 8u * 1024u * 1024u;
    std::vector<uint8_t> body = readAll(source, 10, std::min(size, MaxTag));
    if ((flags & 0x80) && major == 3) body = deunsync(body.data(), body.size());

    size_t pos = 0;
    if (flags & 0x40) {
        if (body.size() < 4) return total;
        const uint32_t extSize = major == 4 ? syncsafe(body.data()) : be32(body.data()) + 4;
        pos = std::min<size_t>(extSize, body.size());
    }

    while (pos + 10 <= body.size()) {
        const uint8_t *f = body.data() + pos;
        if (f[0] == 0) break;
        const std::string id(reinterpret_cast<const char *>(f), 4);
        uint32_t frameSize = major == 4 ? syncsafe(f + 4) : be32(f + 4);
        const uint16_t frameFlags = uint16_t((f[8] << 8) | f[9]);
        pos += 10;
        if (frameSize == 0 || pos + frameSize > body.size()) break;
        std::vector<uint8_t> frame;
        const uint8_t *data = body.data() + pos;
        size_t n = frameSize;
        if (major == 4 && (frameFlags & 0x02)) {
            frame = deunsync(data, n);
            data = frame.data();
            n = frame.size();
        }
        if (major == 4 && (frameFlags & 0x01) && n >= 4) {
            data += 4;
            n -= 4;
        }
        pos += frameSize;

        if (wantText && id[0] == 'T' && id != "TXXX" && n >= 1) {
            const std::string text = decodeText(data + 1, n - 1, data[0]);
            if (id == "TIT2") tags.title = text;
            else if (id == "TPE1") tags.artist = text;
            else if (id == "TPE2") tags.albumArtist = text;
            else if (id == "TALB") tags.album = text;
            else if (id == "TYER" || id == "TDRC" || id == "TDRL") { if (tags.year.empty()) tags.year = text.substr(0, 4); }
            else if (id == "TRCK") tags.track = parseLeadingInt(text);
        } else if (wantPicture && id == "APIC" && n >= 4 && tags.picture.empty()) {
            const int encoding = data[0];
            size_t i = 1;
            std::string mime;
            while (i < n && data[i] != 0) mime += char(data[i++]);
            if (i < n) ++i;
            if (i >= n) continue;
            const int pictureType = data[i++];
            size_t used = 0;
            decodeText(data + i, n - i, encoding, &used);
            i += used;
            if (i >= n) continue;
            // Prefer the front cover (3) but take whatever comes first.
            if (tags.picture.empty() || pictureType == 3) {
                tags.picture.mime = lower(mime);
                tags.picture.data.assign(data + i, data + n);
            }
        }
    }
    return total;
}

void readId3v1(ByteSource &source, Tags &tags)
{
    const uint64_t size = source.size();
    if (size < 128) return;
    uint8_t t[128];
    if (source.read(size - 128, t, 128) != 128 || std::memcmp(t, "TAG", 3) != 0) return;
    auto field = [&](int offset, int length) { return decodeText(t + offset, length, 0); };
    if (tags.title.empty()) tags.title = field(3, 30);
    if (tags.artist.empty()) tags.artist = field(33, 30);
    if (tags.album.empty()) tags.album = field(63, 30);
    if (tags.year.empty()) tags.year = field(93, 4);
    if (tags.track == 0 && t[125] == 0 && t[126] != 0) tags.track = t[126];
}

const int BitrateTable[2][3][16] = {
    // MPEG-1: layer 1, 2, 3
    {{0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0},
     {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0},
     {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0}},
    // MPEG-2 and 2.5
    {{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0},
     {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
     {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0}}};
const int SampleRateTable[4][3] = {{11025, 12000, 8000}, {0, 0, 0}, {22050, 24000, 16000}, {44100, 48000, 32000}};

void readMp3Duration(ByteSource &source, uint32_t tagSize, Tags &tags)
{
    const uint64_t fileSize = source.size();
    if (fileSize <= tagSize + 4) return;
    std::vector<uint8_t> buf = readAll(source, tagSize, 4096);
    for (size_t i = 0; i + 4 <= buf.size(); ++i) {
        const uint8_t *h = buf.data() + i;
        if (h[0] != 0xFF || (h[1] & 0xE0) != 0xE0) continue;
        const int versionBits = (h[1] >> 3) & 3;
        const int layerBits = (h[1] >> 1) & 3;
        const int bitrateIndex = h[2] >> 4;
        const int rateIndex = (h[2] >> 2) & 3;
        if (versionBits == 1 || layerBits == 0 || bitrateIndex == 0 || bitrateIndex == 15 || rateIndex == 3) continue;
        const bool mpeg1 = versionBits == 3;
        const int layer = 4 - layerBits;
        const int bitrate = BitrateTable[mpeg1 ? 0 : 1][layer - 1][bitrateIndex] * 1000;
        const int sampleRate = SampleRateTable[versionBits][rateIndex];
        if (bitrate == 0 || sampleRate == 0) continue;
        const int channelMode = h[3] >> 6;
        const int samplesPerFrame = layer == 1 ? 384 : (layer == 2 || mpeg1) ? 1152 : 576;
        tags.sampleRate = sampleRate;
        tags.channels = channelMode == 3 ? 1 : 2;

        // Xing/Info header for VBR files.
        const size_t sideInfo = mpeg1 ? (channelMode == 3 ? 17 : 32) : (channelMode == 3 ? 9 : 17);
        const size_t xing = i + 4 + sideInfo;
        if (xing + 16 <= buf.size() && (std::memcmp(buf.data() + xing, "Xing", 4) == 0 || std::memcmp(buf.data() + xing, "Info", 4) == 0)) {
            const uint32_t xflags = be32(buf.data() + xing + 4);
            if (xflags & 1) {
                const uint32_t frames = be32(buf.data() + xing + 8);
                tags.durationSeconds = int((uint64_t(frames) * samplesPerFrame + sampleRate / 2) / sampleRate);
                return;
            }
        }
        tags.durationSeconds = int((fileSize - tagSize) * 8 / uint64_t(bitrate));
        return;
    }
}

// Ogg: walks pages from the start, concatenating packets, until it has the
// identification and comment headers.
struct OggHeaders {
    bool isVorbis = false;
    bool isOpus = false;
    int sampleRate = 0;
    int channels = 0;
    int preSkip = 0;
    std::vector<uint8_t> comment;
};

bool readOggHeaders(ByteSource &source, OggHeaders &out)
{
    uint64_t offset = 0;
    std::vector<uint8_t> packet;
    int packetsDone = 0;
    for (int page = 0; page < 64 && packetsDone < 2; ++page) {
        uint8_t header[27];
        if (source.read(offset, header, 27) != 27 || std::memcmp(header, "OggS", 4) != 0) return page > 0 && packetsDone >= 1;
        const int segments = header[26];
        std::vector<uint8_t> table = readAll(source, offset + 27, segments);
        if (table.size() != size_t(segments)) return false;
        uint64_t dataOffset = offset + 27 + segments;
        for (int s = 0; s < segments && packetsDone < 2; ++s) {
            const int len = table[s];
            std::vector<uint8_t> chunk = readAll(source, dataOffset, len);
            dataOffset += len;
            packet.insert(packet.end(), chunk.begin(), chunk.end());
            if (len < 255) {
                if (packetsDone == 0) {
                    if (packet.size() >= 30 && std::memcmp(packet.data(), "\x01vorbis", 7) == 0) {
                        out.isVorbis = true;
                        out.channels = packet[11];
                        out.sampleRate = int(le32(packet.data() + 12));
                    } else if (packet.size() >= 19 && std::memcmp(packet.data(), "OpusHead", 8) == 0) {
                        out.isOpus = true;
                        out.channels = packet[9];
                        out.preSkip = le16(packet.data() + 10);
                        out.sampleRate = 48000;
                    } else {
                        return false;
                    }
                } else {
                    out.comment = packet;
                }
                packet.clear();
                ++packetsDone;
            }
        }
        offset = dataOffset;
    }
    return packetsDone >= 1;
}

void parseVorbisComment(const std::vector<uint8_t> &c, size_t pos, Tags &tags, bool wantPicture, bool wantText)
{
    if (pos + 4 > c.size()) return;
    const uint32_t vendorLen = le32(c.data() + pos);
    pos += 4 + vendorLen;
    if (pos + 4 > c.size()) return;
    const uint32_t count = le32(c.data() + pos);
    pos += 4;
    for (uint32_t i = 0; i < count && pos + 4 <= c.size(); ++i) {
        const uint32_t len = le32(c.data() + pos);
        pos += 4;
        if (pos + len > c.size()) break;
        const std::string entry(reinterpret_cast<const char *>(c.data() + pos), len);
        pos += len;
        const size_t eq = entry.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = lower(entry.substr(0, eq));
        const std::string value = trimmed(entry.substr(eq + 1));
        if (wantText) {
            if (key == "title") tags.title = value;
            else if (key == "artist") tags.artist = value;
            else if (key == "albumartist" || key == "album_artist") tags.albumArtist = value;
            else if (key == "album") tags.album = value;
            else if (key == "date" || key == "year") { if (tags.year.empty()) tags.year = value.substr(0, 4); }
            else if (key == "tracknumber") tags.track = parseLeadingInt(value);
        }
        if (wantPicture && key == "metadata_block_picture" && tags.picture.empty()) {
            const std::vector<uint8_t> block = base64Decode(value);
            // FLAC picture block: type, mime len, mime, desc len, desc, 4x u32, data len, data.
            if (block.size() < 8) continue;
            size_t p = 4;
            const uint32_t mimeLen = be32(block.data() + p);
            p += 4;
            if (p + mimeLen + 4 > block.size()) continue;
            std::string mime(reinterpret_cast<const char *>(block.data() + p), mimeLen);
            p += mimeLen;
            const uint32_t descLen = be32(block.data() + p);
            p += 4 + descLen + 16;
            if (p + 4 > block.size()) continue;
            const uint32_t dataLen = be32(block.data() + p);
            p += 4;
            if (p + dataLen > block.size()) continue;
            tags.picture.mime = lower(mime);
            tags.picture.data.assign(block.begin() + p, block.begin() + p + dataLen);
        }
    }
}

void readOggDuration(ByteSource &source, const OggHeaders &h, Tags &tags)
{
    const uint64_t size = source.size();
    const size_t window = size_t(std::min<uint64_t>(size, 65536));
    std::vector<uint8_t> tail = readAll(source, size - window, window);
    for (size_t i = tail.size(); i >= 27 + 14;) {
        --i;
        if (i + 27 > tail.size()) continue;
        if (std::memcmp(tail.data() + i, "OggS", 4) != 0) continue;
        const uint64_t granule = le64(tail.data() + i + 6);
        if (granule == ~uint64_t(0) || h.sampleRate == 0) return;
        const uint64_t samples = granule > uint64_t(h.preSkip) ? granule - h.preSkip : 0;
        tags.durationSeconds = int((samples + h.sampleRate / 2) / h.sampleRate);
        return;
    }
}

bool readOgg(ByteSource &source, Tags &tags, bool wantPicture, bool wantText)
{
    OggHeaders h;
    if (!readOggHeaders(source, h)) return false;
    tags.sampleRate = h.sampleRate;
    tags.channels = h.channels;
    if (!h.comment.empty()) {
        if (h.isVorbis && h.comment.size() > 7 && std::memcmp(h.comment.data(), "\x03vorbis", 7) == 0) {
            parseVorbisComment(h.comment, 7, tags, wantPicture, wantText);
        } else if (h.isOpus && h.comment.size() > 8 && std::memcmp(h.comment.data(), "OpusTags", 8) == 0) {
            parseVorbisComment(h.comment, 8, tags, wantPicture, wantText);
        }
    }
    if (wantText) readOggDuration(source, h, tags);
    return true;
}

bool readWav(ByteSource &source, Tags &tags, bool wantText)
{
    uint8_t riff[12];
    if (source.read(0, riff, 12) != 12 || std::memcmp(riff, "RIFF", 4) != 0 || std::memcmp(riff + 8, "WAVE", 4) != 0) return false;
    uint64_t pos = 12;
    const uint64_t size = source.size();
    int rate = 0;
    int channels = 0;
    int bits = 0;
    uint32_t dataSize = 0;
    while (pos + 8 <= size) {
        uint8_t ch[8];
        if (source.read(pos, ch, 8) != 8) break;
        const uint32_t len = le32(ch + 4);
        if (std::memcmp(ch, "fmt ", 4) == 0 && len >= 16) {
            uint8_t fmt[16];
            if (source.read(pos + 8, fmt, 16) == 16) {
                channels = le16(fmt + 2);
                rate = int(le32(fmt + 4));
                bits = le16(fmt + 14);
            }
        } else if (std::memcmp(ch, "data", 4) == 0) {
            dataSize = len;
        } else if (std::memcmp(ch, "LIST", 4) == 0 && wantText) {
            std::vector<uint8_t> list = readAll(source, pos + 8, std::min<uint32_t>(len, 65536));
            if (list.size() >= 4 && std::memcmp(list.data(), "INFO", 4) == 0) {
                size_t p = 4;
                while (p + 8 <= list.size()) {
                    const std::string id(reinterpret_cast<const char *>(list.data() + p), 4);
                    const uint32_t n = le32(list.data() + p + 4);
                    p += 8;
                    if (p + n > list.size()) break;
                    const std::string value = decodeText(list.data() + p, n, 3);
                    if (id == "INAM") tags.title = value;
                    else if (id == "IART") tags.artist = value;
                    else if (id == "IPRD") tags.album = value;
                    else if (id == "ICRD") tags.year = value.substr(0, 4);
                    else if (id == "ITRK") tags.track = parseLeadingInt(value);
                    p += n + (n & 1);
                }
            }
        }
        pos += 8 + len + (len & 1);
    }
    tags.sampleRate = rate;
    tags.channels = channels;
    if (rate > 0 && channels > 0 && bits > 0) {
        tags.durationSeconds = int(dataSize / uint64_t(rate * channels * (bits / 8)));
    }
    return true;
}

} // namespace

size_t MemorySource::read(uint64_t offset, void *buffer, size_t n)
{
    if (offset >= bytes.size()) return 0;
    const size_t got = std::min(n, size_t(bytes.size() - offset));
    std::memcpy(buffer, bytes.data() + offset, got);
    return got;
}

Format formatFromName(const std::string &fileName)
{
    const size_t dot = fileName.rfind('.');
    if (dot == std::string::npos) return Format::Unknown;
    const std::string ext = lower(fileName.substr(dot + 1));
    if (ext == "mp3") return Format::Mp3;
    if (ext == "ogg" || ext == "oga") return Format::OggVorbis;
    if (ext == "opus") return Format::Opus;
    if (ext == "wav") return Format::Wav;
    return Format::Unknown;
}

const char *formatName(Format format)
{
    switch (format) {
    case Format::Mp3: return "MP3";
    case Format::OggVorbis: return "Ogg Vorbis";
    case Format::Opus: return "Opus";
    case Format::Wav: return "WAV";
    default: return "Unknown";
    }
}

bool readTags(ByteSource &source, Format format, Tags &out)
{
    switch (format) {
    case Format::Mp3: {
        const uint32_t tagSize = readId3v2(source, out, true, true);
        readId3v1(source, out);
        readMp3Duration(source, tagSize, out);
        return true;
    }
    case Format::OggVorbis:
    case Format::Opus:
        return readOgg(source, out, true, true);
    case Format::Wav:
        return readWav(source, out, true);
    default:
        return false;
    }
}

bool readPicture(ByteSource &source, Format format, Picture &out)
{
    Tags tags;
    switch (format) {
    case Format::Mp3: readId3v2(source, tags, true, false); break;
    case Format::OggVorbis:
    case Format::Opus: readOgg(source, tags, true, false); break;
    default: return false;
    }
    out = std::move(tags.picture);
    return !out.empty();
}

std::vector<uint8_t> base64Decode(const std::string &text)
{
    std::vector<uint8_t> out;
    uint32_t acc = 0;
    int bits = 0;
    for (char c : text) {
        int v;
        if (c >= 'A' && c <= 'Z') v = c - 'A';
        else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
        else if (c >= '0' && c <= '9') v = c - '0' + 52;
        else if (c == '+' || c == '-') v = 62;
        else if (c == '/' || c == '_') v = 63;
        else if (c == '=') break;
        else continue;
        acc = (acc << 6) | uint32_t(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(uint8_t((acc >> bits) & 0xFF));
        }
    }
    return out;
}

} // namespace core
