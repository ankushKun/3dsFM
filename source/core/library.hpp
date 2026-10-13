#pragma once

#include "tags.hpp"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

// The music library: everything found under one folder on the SD card,
// grouped into albums. Portable; the console passes in a directory lister
// and a tag reader so the host tests can use fake files.
namespace core {

struct Entry {
    std::string name;
    bool isDirectory = false;
};

using Lister = std::function<std::vector<Entry>(const std::string &directory)>;
using TagReader = std::function<bool(const std::string &path, Format format, Tags &out)>;

struct Track {
    std::string path;
    std::string title;
    std::string artist;
    std::string album;
    std::string albumKey;
    int trackNumber = 0;
    int durationSeconds = 0;
    Format format = Format::Unknown;
    bool hasEmbeddedArt = false;
    // Hash of the embedded picture (its first bytes and its size), so
    // identical covers can be told apart from merely similar ones without
    // decoding them. 0 when none.
    uint64_t artHash = 0;
    // Kept so a track read back from the index can rebuild its album.
    std::string albumArtist;
    std::string year;
};

struct Album {
    std::string key;
    std::string title;
    std::string artist;
    std::string year;
    std::string folder;
    // Where to get the cover: a loose image file in the album folder, or a
    // track with embedded art. Empty when there is none.
    std::string artPath;
    bool artIsEmbedded = false;
    std::vector<int> tracks; // indices into Library::tracks()
    int durationSeconds = 0;
    // A folder of songs by different artists is a playlist, not an album:
    // it has no single cover and is shown as a mosaic of its songs' art.
    bool isPlaylist = false;
    // Loose songs straight in the music folder, one per album, gathered
    // under their artist: "Singles". Drawn as a mosaic like a playlist.
    bool isSingles = false;
    // For a playlist or singles: up to four tracks with distinct covers.
    std::vector<int> mosaicTracks;
};

// A loose cover picture: the folder it sits in and its path.
using Cover = std::pair<std::string, std::string>;

// The on-disk index: the tracks and cover files of the last scan, so the
// next launch can show the whole library before it reads a single music
// file. One line per track or cover; see the .cpp for the format.
std::string encodeIndex(const std::vector<Track> &tracks, const std::vector<Cover> &covers = {});
// The pieces of an index, so a scan can write the header once and append
// each batch of tracks as it goes instead of rewriting the whole file.
std::string encodeIndexHeader();
std::string encodeIndexTracks(const std::vector<Track> &tracks, size_t from, size_t to);
std::string encodeIndexCovers(const std::vector<Cover> &covers);
bool decodeIndex(const std::string &text, std::vector<Track> &tracks, std::vector<Cover> &covers);
bool decodeIndex(const std::string &text, std::vector<Track> &tracks);
// The format version an index was written with (0 when it is not one), and
// the version this scanner writes. An older index still shows the library
// while every file is read again under the newer rules.
int indexVersion(const std::string &text);
int currentIndexVersion();

class Library {
public:
    // Lists every playable file under root. Cheap; no tags are read yet.
    void collect(const std::string &root, const Lister &lister);
    // Tracks from a previous scan: a collected file with the same path is
    // taken from here instead of having its tags read. Call before step.
    void useIndex(const std::vector<Track> &known);
    // Reads tags for up to `count` collected files (or takes them from the
    // index). Returns false when done.
    bool step(const TagReader &reader, int count);
    // Groups the collected tracks into albums. Call after stepping finishes
    // (it is also safe to call in between for a partial view).
    void build();
    void clear();
    // The whole library straight from the index, with no folder listing and
    // no tag reads: the state of the last scan, ready to browse and play.
    void loadFromIndex(const std::string &root, const std::vector<Track> &tracks, const std::vector<Cover> &covers);
    // Files collect() found that are not music and only clutter the card:
    // macOS "._name" resource forks and .DS_Store, Windows Thumbs.db.
    const std::vector<std::string> &junkFiles() const { return junk_; }

    int collectedCount() const { return int(pending_.size()); }
    int scannedCount() const { return scanned_; }
    // How many of the scanned tracks were taken from the index.
    int reusedCount() const { return reused_; }
    bool scanning() const { return scanned_ < int(pending_.size()); }

    const std::vector<Track> &tracks() const { return tracks_; }
    const std::vector<Album> &albums() const { return albums_; }
    const std::vector<Cover> &looseCovers() const { return looseCovers_; }
    std::vector<std::string> artists() const;
    std::vector<int> albumsByArtist(const std::string &artist) const;
    const Album *albumOf(int trackIndex) const;
    int albumIndex(const std::string &key) const;

    // Track indices whose title, artist or album contain the query,
    // case-insensitively, best matches first.
    std::vector<int> search(const std::string &query) const;

    int totalDurationSeconds() const;

private:
    std::string root_;
    std::vector<std::string> junk_;
    struct Pending {
        std::string path;
        std::string folder;
        std::string fileName;
        Format format;
    };
    std::vector<Pending> pending_;
    std::unordered_map<std::string, Track> index_;
    int scanned_ = 0;
    int reused_ = 0;
    std::vector<Track> tracks_;
    std::vector<Album> albums_;
    std::unordered_map<std::string, int> albumByKey_; // rebuilt by build()
    std::vector<Cover> looseCovers_;
};

// Helpers shared with the UI and the tests.
// Drops the noise video sites put in titles: "(Official Video)", "[Lyrics]",
// "[HD]", a trailing "- Official Audio", an 11-character video id in brackets.
std::string cleanTitle(const std::string &title);
// "Artist - Title" (also with an en or em dash) into its two halves.
bool splitArtistTitle(const std::string &text, std::string &artist, std::string &title);
// A file that is desktop clutter rather than content.
bool isJunkFile(const std::string &name);
std::string fileStem(const std::string &path);
std::string parentFolder(const std::string &path);
std::string baseName(const std::string &path);
std::string toLowerAscii(const std::string &s);
uint64_t hashBytes(const uint8_t *data, size_t size);

} // namespace core
