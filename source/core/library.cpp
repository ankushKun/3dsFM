#include "library.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

namespace core {

std::string toLowerAscii(const std::string &s)
{
    std::string out = s;
    for (char &c : out) {
        if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    }
    return out;
}

uint64_t hashBytes(const uint8_t *data, size_t size)
{
    // FNV-1a, 64-bit.
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < size; ++i) {
        h ^= data[i];
        h *= 1099511628211ull;
    }
    return size == 0 ? 0 : h;
}

std::string baseName(const std::string &path)
{
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string parentFolder(const std::string &path)
{
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

std::string fileStem(const std::string &path)
{
    const std::string name = baseName(path);
    const size_t dot = name.rfind('.');
    return dot == std::string::npos || dot == 0 ? name : name.substr(0, dot);
}

namespace {

std::string trimSpaces(const std::string &s)
{
    size_t a = 0;
    size_t b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) --b;
    return s.substr(a, b - a);
}

// Words that, on their own inside brackets or after a final dash, say
// nothing about the song.
bool isNoiseWord(const std::string &w)
{
    static const char *const words[] = {"official", "oficial", "video", "videoclip", "music", "audio", "lyric", "lyrics", "hd", "hq", "4k",
                                        "1080p", "720p", "mv", "m/v", "visualizer", "visualiser", "clip", "version", "ver", "full", "song",
                                        "with", "subtitles", "sub", "subs", "eng", "english", "cc", "the", "from", "ost", "stream", "quality"};
    for (const char *x : words) {
        if (w == x) return true;
    }
    return false;
}

// True when every word of the text is noise, or it is a video id.
bool isNoise(const std::string &text)
{
    const std::string t = toLowerAscii(trimSpaces(text));
    if (t.empty()) return false;
    if (t.size() == 11) {
        bool id = true;
        for (char c : t) {
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) id = false;
        }
        // An id only counts as noise when it has a digit or an underscore or
        // a dash in it; a plain eleven-letter word is just a word.
        if (id && t.find_first_of("0123456789_-") != std::string::npos) return true;
    }
    // "(version)" and "(full)" alone are meaningful; they count as noise only
    // next to other noise, such as "(Full Video Version)".
    bool sawStrongNoise = false;
    size_t start = 0;
    int words = 0;
    while (start <= t.size()) {
        size_t end = t.find_first_of(" -/", start);
        if (end == std::string::npos) end = t.size();
        const std::string w = t.substr(start, end - start);
        if (!w.empty()) {
            ++words;
            if (!isNoiseWord(w)) return false;
            if (w != "version" && w != "ver" && w != "full" && w != "the" && w != "with" && w != "from" && w != "song") sawStrongNoise = true;
        }
        start = end + 1;
    }
    return words > 0 && sawStrongNoise;
}

bool isCoverFile(const std::string &name)
{
    const std::string n = toLowerAscii(name);
    const size_t dot = n.rfind('.');
    if (dot == std::string::npos) return false;
    const std::string ext = n.substr(dot + 1);
    if (ext != "jpg" && ext != "jpeg" && ext != "png") return false;
    const std::string stem = n.substr(0, dot);
    return stem == "cover" || stem == "folder" || stem == "front" || stem == "album" || stem == "albumart";
}

// "03 - Title" or "03. Title" or "03 Title": strips a leading track number.
// With punctuationOnly, a number followed by just a space stays, so a loose
// "5 Seconds of Summer - ..." or "21 Pilots - ..." keeps its name.
std::string stripLeadingNumber(const std::string &stem, int &number, bool punctuationOnly = false)
{
    size_t i = 0;
    int value = 0;
    while (i < stem.size() && stem[i] >= '0' && stem[i] <= '9' && i < 3) {
        value = value * 10 + (stem[i] - '0');
        ++i;
    }
    if (i == 0 || i >= stem.size()) return stem;
    size_t j = i;
    bool punctuation = false;
    while (j < stem.size() && (stem[j] == ' ' || stem[j] == '-' || stem[j] == '.' || stem[j] == '_')) {
        if (stem[j] != ' ') punctuation = true;
        ++j;
    }
    if (j == i || j >= stem.size()) return stem;
    if (punctuationOnly && !punctuation) return stem;
    number = value;
    return stem.substr(j);
}

} // namespace

std::string cleanTitle(const std::string &title)
{
    std::string out = title;
    // Bracketed groups that are pure noise.
    for (int pass = 0; pass < 6; ++pass) {
        bool changed = false;
        for (const char *pair : {"()", "[]", "{}"}) {
            size_t close = out.find(pair[1]);
            while (close != std::string::npos) {
                const size_t open = out.rfind(pair[0], close);
                if (open != std::string::npos && isNoise(out.substr(open + 1, close - open - 1))) {
                    out.erase(open, close - open + 1);
                    changed = true;
                    break;
                }
                close = out.find(pair[1], close + 1);
            }
        }
        if (!changed) break;
    }
    // A final " - Official Video" style tail.
    for (const char *dash : {" - ", " \xe2\x80\x93 ", " \xe2\x80\x94 ", " | "}) {
        const size_t at = out.rfind(dash);
        if (at != std::string::npos && at > 0 && isNoise(out.substr(at + std::strlen(dash)))) out.erase(at);
    }
    // Tidy the spaces the removals leave behind.
    std::string tidy;
    for (char c : out) {
        if ((c == ' ' || c == '\t') && (tidy.empty() || tidy.back() == ' ')) continue;
        tidy += c == '\t' ? ' ' : c;
    }
    return trimSpaces(tidy);
}

bool splitArtistTitle(const std::string &text, std::string &artist, std::string &title)
{
    for (const char *dash : {" - ", " \xe2\x80\x93 ", " \xe2\x80\x94 "}) {
        const size_t at = text.find(dash);
        if (at == std::string::npos) continue;
        const std::string a = trimSpaces(text.substr(0, at));
        const std::string t = trimSpaces(text.substr(at + std::strlen(dash)));
        if (a.empty() || t.empty()) continue;
        artist = a;
        title = t;
        return true;
    }
    return false;
}

bool isJunkFile(const std::string &name)
{
    if (name.rfind("._", 0) == 0) return true;
    const std::string n = toLowerAscii(name);
    return n == ".ds_store" || n == "thumbs.db" || n == "desktop.ini";
}

namespace {

bool naturalLess(const std::string &a, const std::string &b)
{
    return toLowerAscii(a) < toLowerAscii(b);
}

} // namespace

void Library::clear()
{
    pending_.clear();
    junk_.clear();
    index_.clear();
    scanned_ = 0;
    reused_ = 0;
    tracks_.clear();
    albums_.clear();
    looseCovers_.clear();
}

void Library::collect(const std::string &root, const Lister &lister)
{
    clear();
    root_ = root;
    std::vector<std::string> stack{root};
    int visited = 0;
    while (!stack.empty() && visited < 2000) {
        const std::string dir = stack.back();
        stack.pop_back();
        ++visited;
        std::vector<Entry> entries = lister(dir);
        std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) { return naturalLess(a.name, b.name); });
        for (const Entry &e : entries) {
            if (e.name.empty()) continue;
            const std::string path = dir + "/" + e.name;
            if (!e.isDirectory && isJunkFile(e.name)) {
                junk_.push_back(path);
                continue;
            }
            if (e.name[0] == '.') continue;
            if (e.isDirectory) {
                stack.push_back(path);
                continue;
            }
            const Format format = formatFromName(e.name);
            if (format != Format::Unknown) {
                pending_.push_back({path, dir, e.name, format});
            } else if (isCoverFile(e.name)) {
                looseCovers_.emplace_back(dir, path);
            }
        }
    }
}

void Library::useIndex(const std::vector<Track> &known)
{
    index_.clear();
    for (const Track &t : known) index_[t.path] = t;
}

bool Library::step(const TagReader &reader, int count)
{
    for (int i = 0; i < count && scanned_ < int(pending_.size()); ++i, ++scanned_) {
        const Pending &p = pending_[scanned_];
        Track t;
        const auto known = index_.find(p.path);
        if (known != index_.end()) {
            t = known->second;
            t.format = p.format;
            ++reused_;
        } else {
            Tags tags;
            reader(p.path, p.format, tags);

            t.path = p.path;
            t.format = p.format;
            t.durationSeconds = tags.durationSeconds;
            t.trackNumber = tags.track;
            t.hasEmbeddedArt = !tags.picture.empty();
            t.artHash = hashBytes(tags.picture.data.data(), tags.picture.data.size());
            if (t.artHash != 0 && tags.pictureBytes != 0) t.artHash ^= tags.pictureBytes * 1099511628211ull;
            t.year = tags.year;

            // The title and artist: from the tags, else from a file name of
            // the "Artist - Title" kind, else the file name as the title.
            // Either way the video-site noise comes off the title.
            const bool loose = p.folder == root_;
            int numberFromName = 0;
            const std::string stem = stripLeadingNumber(fileStem(p.fileName), numberFromName, loose);
            std::string title = cleanTitle(tags.title);
            std::string artist = trimSpaces(tags.artist);
            std::string a;
            std::string b;
            if (artist.empty() && splitArtistTitle(title, a, b)) {
                artist = a;
                title = cleanTitle(b);
            }
            if ((title.empty() || artist.empty()) && splitArtistTitle(stem, a, b)) {
                if (artist.empty()) artist = a;
                if (title.empty()) title = cleanTitle(b);
            }
            if (title.empty()) title = cleanTitle(stem);
            if (title.empty()) title = stem;
            t.title = title;
            if (t.trackNumber == 0) t.trackNumber = numberFromName;

            // A loose file sits straight in the music folder; the folder
            // names say nothing about it. In a subfolder the folder is the
            // album and its parent the artist, the usual Artist/Album layout.
            const std::string folderName = baseName(p.folder);
            const std::string parentName = baseName(parentFolder(p.folder));
            if (artist.empty()) artist = (loose || parentName.empty() || parentName.back() == ':') ? "Unknown artist" : parentName;
            t.artist = artist;
            const std::string album = cleanTitle(tags.album);
            t.album = !album.empty() ? album : loose ? t.title : folderName;
            t.albumArtist = trimSpaces(tags.albumArtist).empty() ? t.artist : trimSpaces(tags.albumArtist);
            // Albums are grouped by name and folder, so two albums with the same
            // title by different artists in different folders stay apart, and a
            // compilation in one folder stays together. A loose single is its
            // own album; build() gathers those under the artist.
            t.albumKey = toLowerAscii(t.album) + "|" + toLowerAscii(album.empty() && !loose ? p.folder : t.albumArtist);
        }
        tracks_.push_back(std::move(t));
    }
    return scanned_ < int(pending_.size());
}

void Library::loadFromIndex(const std::string &root, const std::vector<Track> &tracks, const std::vector<Cover> &covers)
{
    clear();
    root_ = root;
    tracks_ = tracks;
    looseCovers_ = covers;
    build();
}

// Index format: a header line, then one line per track with fields
// separated by tabs (tabs and newlines inside a field become spaces):
// path, format, title, artist, album, album artist, year, album key, track
// number, duration, has embedded art, art hash (hex). A cover file is a
// line of "cover", its folder and its path. The version goes up whenever
// the scanner reads files differently, so an old index is read again once.
namespace {

const char *const IndexHeader = "3dsFM library index 3";
const char *const IndexHeaderStem = "3dsFM library index ";
const int IndexVersion = 3;

std::string indexField(const std::string &s)
{
    std::string out = s;
    for (char &c : out) {
        if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    }
    return out;
}

} // namespace

std::string encodeIndexHeader()
{
    return std::string(IndexHeader) + '\n';
}

std::string encodeIndexTracks(const std::vector<Track> &tracks, size_t from, size_t to)
{
    std::string out;
    char number[40];
    for (size_t i = from; i < to && i < tracks.size(); ++i) {
        const Track &t = tracks[i];
        out += indexField(t.path) + '\t';
        out += std::to_string(int(t.format)) + '\t';
        out += indexField(t.title) + '\t';
        out += indexField(t.artist) + '\t';
        out += indexField(t.album) + '\t';
        out += indexField(t.albumArtist) + '\t';
        out += indexField(t.year) + '\t';
        out += indexField(t.albumKey) + '\t';
        out += std::to_string(t.trackNumber) + '\t';
        out += std::to_string(t.durationSeconds) + '\t';
        out += (t.hasEmbeddedArt ? "1" : "0");
        out += '\t';
        std::snprintf(number, sizeof(number), "%llx", static_cast<unsigned long long>(t.artHash));
        out += number;
        out += '\n';
    }
    return out;
}

std::string encodeIndexCovers(const std::vector<Cover> &covers)
{
    std::string out;
    for (const Cover &c : covers) out += "cover\t" + indexField(c.first) + '\t' + indexField(c.second) + '\n';
    return out;
}

std::string encodeIndex(const std::vector<Track> &tracks, const std::vector<Cover> &covers)
{
    return encodeIndexHeader() + encodeIndexTracks(tracks, 0, tracks.size()) + encodeIndexCovers(covers);
}

int indexVersion(const std::string &text)
{
    const std::string stem = IndexHeaderStem;
    if (text.rfind(stem, 0) != 0) return 0;
    const int version = std::atoi(text.c_str() + stem.size());
    return version > 0 ? version : 0;
}

int currentIndexVersion()
{
    return IndexVersion;
}

bool decodeIndex(const std::string &text, std::vector<Track> &tracks)
{
    std::vector<Cover> covers;
    return decodeIndex(text, tracks, covers);
}

bool decodeIndex(const std::string &text, std::vector<Track> &tracks, std::vector<Cover> &covers)
{
    tracks.clear();
    covers.clear();
    size_t pos = 0;
    auto nextLine = [&](std::string &line) {
        if (pos >= text.size()) return false;
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        line = text.substr(pos, end - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        pos = end + 1;
        return true;
    };
    std::string line;
    if (!nextLine(line) || indexVersion(line) < 1 || indexVersion(line) > IndexVersion) return false;
    while (nextLine(line)) {
        if (line.empty()) continue;
        std::vector<std::string> fields;
        size_t start = 0;
        while (true) {
            const size_t tab = line.find('\t', start);
            fields.push_back(line.substr(start, tab == std::string::npos ? std::string::npos : tab - start));
            if (tab == std::string::npos) break;
            start = tab + 1;
        }
        if (fields.size() == 3 && fields[0] == "cover") {
            if (!fields[1].empty() && !fields[2].empty()) covers.emplace_back(fields[1], fields[2]);
            continue;
        }
        if (fields.size() != 12) continue;
        Track t;
        t.path = fields[0];
        t.format = Format(std::atoi(fields[1].c_str()));
        t.title = fields[2];
        t.artist = fields[3];
        t.album = fields[4];
        t.albumArtist = fields[5];
        t.year = fields[6];
        t.albumKey = fields[7];
        t.trackNumber = std::atoi(fields[8].c_str());
        t.durationSeconds = std::atoi(fields[9].c_str());
        t.hasEmbeddedArt = fields[10] == "1";
        t.artHash = std::strtoull(fields[11].c_str(), nullptr, 16);
        if (t.path.empty() || t.albumKey.empty()) continue;
        tracks.push_back(std::move(t));
    }
    return true;
}

void Library::build()
{
    // A loose file (straight in the music folder) whose album holds only
    // that one song is a single. An artist's singles share one "Singles"
    // collection, so a folder of hundreds of downloaded songs becomes a
    // tile per artist rather than a tile per song. The key is written back
    // to the track, so the index remembers it and a reload builds the same
    // library; the song keeps its own album name for the lists.
    static const char *const SinglesPrefix = "singles|";
    std::map<std::string, int> perKey;
    for (const Track &t : tracks_) ++perKey[t.albumKey];
    for (Track &t : tracks_) {
        if (t.albumKey.rfind(SinglesPrefix, 0) == 0) continue;
        if (perKey[t.albumKey] == 1 && parentFolder(t.path) == root_) t.albumKey = SinglesPrefix + toLowerAscii(t.albumArtist);
    }

    albums_.clear();
    std::map<std::string, int> byKey;
    for (int i = 0; i < int(tracks_.size()); ++i) {
        const Track &t = tracks_[i];
        auto it = byKey.find(t.albumKey);
        if (it == byKey.end()) {
            Album a;
            a.key = t.albumKey;
            a.isSingles = t.albumKey.rfind(SinglesPrefix, 0) == 0;
            a.title = a.isSingles ? "Singles" : t.album;
            a.artist = t.albumArtist;
            a.year = a.isSingles ? "" : t.year;
            a.folder = a.isSingles ? root_ : parentFolder(t.path);
            albums_.push_back(std::move(a));
            it = byKey.emplace(t.albumKey, int(albums_.size()) - 1).first;
        }
        Album &a = albums_[it->second];
        if (a.year.empty() && !a.isSingles) a.year = t.year;
        a.tracks.push_back(i);
        a.durationSeconds += t.durationSeconds;
    }
    for (Album &a : albums_) {
        std::sort(a.tracks.begin(), a.tracks.end(), [&](int x, int y) {
            const Track &tx = tracks_[x];
            const Track &ty = tracks_[y];
            if (tx.trackNumber != ty.trackNumber) {
                if (tx.trackNumber == 0) return false;
                if (ty.trackNumber == 0) return true;
                return tx.trackNumber < ty.trackNumber;
            }
            return naturalLess(tx.title, ty.title);
        });
        for (const auto &cover : looseCovers_) {
            if (!a.isSingles && cover.first == a.folder) {
                a.artPath = cover.second;
                a.artIsEmbedded = false;
                break;
            }
        }
        if (a.artPath.empty()) {
            for (int t : a.tracks) {
                if (tracks_[t].hasEmbeddedArt) {
                    a.artPath = tracks_[t].path;
                    a.artIsEmbedded = true;
                    break;
                }
            }
        }
        // An artist of "Various" style folders: if every track has a
        // different artist, call the album's artist "Various artists".
        a.isPlaylist = false;
        if (a.tracks.size() > 1 && !a.isSingles) {
            bool allSame = true;
            for (int t : a.tracks) {
                if (toLowerAscii(tracks_[t].artist) != toLowerAscii(tracks_[a.tracks[0]].artist)) { allSame = false; break; }
            }
            if (!allSame) {
                a.isPlaylist = true;
                if (a.artist == tracks_[a.tracks[0]].artist || toLowerAscii(a.artist) == "various artists") a.artist = "Various artists";
            }
        }
        a.mosaicTracks.clear();
        if (a.isPlaylist || a.isSingles) {
            std::vector<uint64_t> seen;
            for (int t : a.tracks) {
                const Track &track = tracks_[t];
                if (!track.hasEmbeddedArt) continue;
                if (std::find(seen.begin(), seen.end(), track.artHash) != seen.end()) continue;
                seen.push_back(track.artHash);
                a.mosaicTracks.push_back(t);
                if (a.mosaicTracks.size() == 4) break;
            }
        }
    }
    std::sort(albums_.begin(), albums_.end(), [](const Album &x, const Album &y) {
        const std::string ax = toLowerAscii(x.artist);
        const std::string ay = toLowerAscii(y.artist);
        if (ax != ay) return ax < ay;
        if (x.isSingles != y.isSingles) return y.isSingles; // an artist's singles come after their albums
        if (x.year != y.year) return x.year < y.year;
        return toLowerAscii(x.title) < toLowerAscii(y.title);
    });
}

std::vector<std::string> Library::artists() const
{
    std::vector<std::string> out;
    for (const Album &a : albums_) {
        if (std::find(out.begin(), out.end(), a.artist) == out.end()) out.push_back(a.artist);
    }
    std::sort(out.begin(), out.end(), naturalLess);
    return out;
}

std::vector<int> Library::albumsByArtist(const std::string &artist) const
{
    std::vector<int> out;
    for (int i = 0; i < int(albums_.size()); ++i) {
        if (albums_[i].artist == artist) out.push_back(i);
    }
    return out;
}

const Album *Library::albumOf(int trackIndex) const
{
    if (trackIndex < 0 || trackIndex >= int(tracks_.size())) return nullptr;
    const int index = albumIndex(tracks_[trackIndex].albumKey);
    return index < 0 ? nullptr : &albums_[index];
}

int Library::albumIndex(const std::string &key) const
{
    for (int i = 0; i < int(albums_.size()); ++i) {
        if (albums_[i].key == key) return i;
    }
    return -1;
}

std::vector<int> Library::search(const std::string &query) const
{
    std::vector<std::pair<int, int>> scored; // score, index
    const std::string q = toLowerAscii(query);
    if (q.empty()) return {};
    for (int i = 0; i < int(tracks_.size()); ++i) {
        const Track &t = tracks_[i];
        const std::string title = toLowerAscii(t.title);
        const std::string artist = toLowerAscii(t.artist);
        const std::string album = toLowerAscii(t.album);
        int score = 0;
        if (title.rfind(q, 0) == 0) score = 4;
        else if (title.find(q) != std::string::npos) score = 3;
        else if (artist.find(q) != std::string::npos) score = 2;
        else if (album.find(q) != std::string::npos) score = 1;
        if (score > 0) scored.emplace_back(score, i);
    }
    std::stable_sort(scored.begin(), scored.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
    std::vector<int> out;
    for (const auto &s : scored) out.push_back(s.second);
    return out;
}

int Library::totalDurationSeconds() const
{
    int total = 0;
    for (const Track &t : tracks_) total += t.durationSeconds;
    return total;
}

} // namespace core
