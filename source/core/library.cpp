#include "library.hpp"

#include <algorithm>
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
std::string stripLeadingNumber(const std::string &stem, int &number)
{
    size_t i = 0;
    int value = 0;
    while (i < stem.size() && stem[i] >= '0' && stem[i] <= '9' && i < 3) {
        value = value * 10 + (stem[i] - '0');
        ++i;
    }
    if (i == 0 || i >= stem.size()) return stem;
    size_t j = i;
    while (j < stem.size() && (stem[j] == ' ' || stem[j] == '-' || stem[j] == '.' || stem[j] == '_')) ++j;
    if (j == i || j >= stem.size()) return stem;
    number = value;
    return stem.substr(j);
}

bool naturalLess(const std::string &a, const std::string &b)
{
    return toLowerAscii(a) < toLowerAscii(b);
}

} // namespace

void Library::clear()
{
    pending_.clear();
    scanned_ = 0;
    tracks_.clear();
    albums_.clear();
    looseCovers_.clear();
}

void Library::collect(const std::string &root, const Lister &lister)
{
    clear();
    std::vector<std::string> stack{root};
    int visited = 0;
    while (!stack.empty() && visited < 2000) {
        const std::string dir = stack.back();
        stack.pop_back();
        ++visited;
        std::vector<Entry> entries = lister(dir);
        std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) { return naturalLess(a.name, b.name); });
        for (const Entry &e : entries) {
            if (e.name.empty() || e.name[0] == '.') continue;
            const std::string path = dir + "/" + e.name;
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

bool Library::step(const TagReader &reader, int count)
{
    for (int i = 0; i < count && scanned_ < int(pending_.size()); ++i, ++scanned_) {
        const Pending &p = pending_[scanned_];
        Tags tags;
        reader(p.path, p.format, tags);

        Track t;
        t.path = p.path;
        t.format = p.format;
        t.durationSeconds = tags.durationSeconds;
        t.trackNumber = tags.track;
        t.hasEmbeddedArt = !tags.picture.empty();
        t.artHash = hashBytes(tags.picture.data.data(), tags.picture.data.size());

        int numberFromName = 0;
        const std::string stemTitle = stripLeadingNumber(fileStem(p.fileName), numberFromName);
        t.title = tags.title.empty() ? stemTitle : tags.title;
        if (t.trackNumber == 0) t.trackNumber = numberFromName;

        const std::string folderName = baseName(p.folder);
        const std::string parentName = baseName(parentFolder(p.folder));
        t.album = tags.album.empty() ? folderName : tags.album;
        t.artist = tags.artist.empty() ? (parentName.empty() ? "Unknown artist" : parentName) : tags.artist;
        const std::string albumArtist = tags.albumArtist.empty() ? t.artist : tags.albumArtist;
        // Albums are grouped by name and folder, so two albums with the same
        // title by different artists in different folders stay apart, and a
        // compilation in one folder stays together.
        t.albumKey = toLowerAscii(t.album) + "|" + toLowerAscii(tags.album.empty() ? p.folder : albumArtist);
        tracks_.push_back(std::move(t));

        // Remember the album's folder and year while we have the tags.
        const int index = albumIndex(tracks_.back().albumKey);
        if (index < 0) {
            Album a;
            a.key = tracks_.back().albumKey;
            a.title = tracks_.back().album;
            a.artist = albumArtist;
            a.year = tags.year;
            a.folder = p.folder;
            albums_.push_back(std::move(a));
        } else if (albums_[index].year.empty()) {
            albums_[index].year = tags.year;
        }
    }
    return scanned_ < int(pending_.size());
}

void Library::build()
{
    for (Album &a : albums_) {
        a.tracks.clear();
        a.durationSeconds = 0;
        a.artPath.clear();
        a.artIsEmbedded = false;
    }
    for (int i = 0; i < int(tracks_.size()); ++i) {
        const int index = albumIndex(tracks_[i].albumKey);
        if (index < 0) continue;
        Album &a = albums_[index];
        a.tracks.push_back(i);
        a.durationSeconds += tracks_[i].durationSeconds;
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
            if (cover.first == a.folder) {
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
        if (a.tracks.size() > 1) {
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
        if (a.isPlaylist) {
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
