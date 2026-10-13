#include "../check.hpp"
#include "library.hpp"

#include <map>
#include <string>
#include <vector>

using namespace core;

namespace {

struct FakeFs {
    std::map<std::string, std::vector<Entry>> dirs;
    std::map<std::string, Tags> tags;

    Lister lister() const
    {
        return [this](const std::string &dir) {
            auto it = dirs.find(dir);
            return it == dirs.end() ? std::vector<Entry>{} : it->second;
        };
    }
    TagReader reader() const
    {
        return [this](const std::string &path, Format, Tags &out) {
            auto it = tags.find(path);
            if (it == tags.end()) return true;
            out = it->second;
            return true;
        };
    }
};

Tags tag(const std::string &title, const std::string &artist, const std::string &album, int track, int secs, const std::string &year = "")
{
    Tags t;
    t.title = title;
    t.artist = artist;
    t.album = album;
    t.track = track;
    t.durationSeconds = secs;
    t.year = year;
    return t;
}

FakeFs sampleFs()
{
    FakeFs fs;
    fs.dirs["sdmc:/Music"] = {{"Miki Matsubara", true}, {"Loose", true}, {".hidden", true}, {"readme.txt", false}};
    fs.dirs["sdmc:/Music/Miki Matsubara"] = {{"Pocket Park", true}};
    fs.dirs["sdmc:/Music/Miki Matsubara/Pocket Park"] = {{"02 - Niji Iro no Ame.mp3", false}, {"01 - Mayonaka no Door.mp3", false}, {"cover.jpg", false}, {"03 Daydream.ogg", false}};
    fs.dirs["sdmc:/Music/Loose"] = {{"untagged tone.wav", false}, {"09. Another.opus", false}};
    fs.tags["sdmc:/Music/Miki Matsubara/Pocket Park/01 - Mayonaka no Door.mp3"] = tag("Mayonaka no Door", "Miki Matsubara", "Pocket Park", 1, 257, "1980");
    fs.tags["sdmc:/Music/Miki Matsubara/Pocket Park/02 - Niji Iro no Ame.mp3"] = tag("Niji Iro no Ame", "Miki Matsubara", "Pocket Park", 2, 242);
    Tags third = tag("Daydream", "Miki Matsubara", "Pocket Park", 3, 235);
    third.picture.data = {1, 2, 3};
    fs.tags["sdmc:/Music/Miki Matsubara/Pocket Park/03 Daydream.ogg"] = third;
    return fs;
}

Library scanned(const FakeFs &fs)
{
    Library lib;
    lib.collect("sdmc:/Music", fs.lister());
    while (lib.step(fs.reader(), 2)) {
    }
    lib.build();
    return lib;
}

} // namespace

TEST("collect walks folders, skips dotfiles and non-music, and counts files")
{
    FakeFs fs = sampleFs();
    Library lib;
    lib.collect("sdmc:/Music", fs.lister());
    CHECK_EQ(lib.collectedCount(), 5);
    CHECK(lib.scanning());
}

TEST("tagged tracks group into an album with the loose cover and a total duration")
{
    Library lib = scanned(sampleFs());
    CHECK_EQ(lib.tracks().size(), size_t(5));
    CHECK_EQ(lib.albums().size(), size_t(2));
    const int index = lib.albumIndex(lib.tracks()[0].albumKey);
    CHECK(index >= 0);
    const Album *pocket = nullptr;
    for (const Album &a : lib.albums()) {
        if (a.title == "Pocket Park") pocket = &a;
    }
    CHECK(pocket != nullptr);
    if (pocket == nullptr) return;
    CHECK_EQ(pocket->artist, std::string("Miki Matsubara"));
    CHECK_EQ(pocket->year, std::string("1980"));
    CHECK_EQ(pocket->tracks.size(), size_t(3));
    CHECK_EQ(pocket->durationSeconds, 257 + 242 + 235);
    CHECK_EQ(pocket->artPath, std::string("sdmc:/Music/Miki Matsubara/Pocket Park/cover.jpg"));
    CHECK(!pocket->artIsEmbedded);
    CHECK_EQ(lib.tracks()[pocket->tracks[0]].title, std::string("Mayonaka no Door"));
    CHECK_EQ(lib.tracks()[pocket->tracks[2]].title, std::string("Daydream"));
}

TEST("untagged files get titles from file names and albums from folders")
{
    Library lib = scanned(sampleFs());
    const Album *loose = nullptr;
    for (const Album &a : lib.albums()) {
        if (a.title == "Loose") loose = &a;
    }
    CHECK(loose != nullptr);
    if (loose == nullptr) return;
    CHECK_EQ(loose->artist, std::string("Music"));
    CHECK_EQ(loose->tracks.size(), size_t(2));
    const Track &first = lib.tracks()[loose->tracks[0]];
    CHECK_EQ(first.title, std::string("Another"));
    CHECK_EQ(first.trackNumber, 9);
    const Track &second = lib.tracks()[loose->tracks[1]];
    CHECK_EQ(second.title, std::string("untagged tone"));
    CHECK_EQ(second.trackNumber, 0);
    CHECK(loose->artPath.empty());
}

TEST("embedded art is used when the folder has no cover file")
{
    FakeFs fs = sampleFs();
    fs.dirs["sdmc:/Music/Miki Matsubara/Pocket Park"] = {{"01 - Mayonaka no Door.mp3", false}, {"03 Daydream.ogg", false}};
    Library lib = scanned(fs);
    for (const Album &a : lib.albums()) {
        if (a.title != "Pocket Park") continue;
        CHECK(a.artIsEmbedded);
        CHECK_EQ(a.artPath, std::string("sdmc:/Music/Miki Matsubara/Pocket Park/03 Daydream.ogg"));
    }
}

TEST("artists and search")
{
    Library lib = scanned(sampleFs());
    const std::vector<std::string> artists = lib.artists();
    CHECK_EQ(artists.size(), size_t(2));
    CHECK_EQ(artists[0], std::string("Miki Matsubara"));
    CHECK_EQ(lib.albumsByArtist("Miki Matsubara").size(), size_t(1));

    std::vector<int> hits = lib.search("niji");
    CHECK_EQ(hits.size(), size_t(1));
    CHECK_EQ(lib.tracks()[hits[0]].title, std::string("Niji Iro no Ame"));
    hits = lib.search("MIKI");
    CHECK_EQ(hits.size(), size_t(3));
    hits = lib.search("pocket");
    CHECK_EQ(hits.size(), size_t(3));
    CHECK(lib.search("").empty());
    CHECK(lib.search("zzz").empty());
}

TEST("albumOf finds the album for a track")
{
    Library lib = scanned(sampleFs());
    const Album *a = lib.albumOf(0);
    CHECK(a != nullptr);
    CHECK(lib.albumOf(99) == nullptr);
}

TEST("a folder of songs by different artists is a playlist")
{
    FakeFs fs = sampleFs();
    fs.dirs["sdmc:/Music/Top Picks"] = {{"01 - One.mp3", false}, {"02 - Two.mp3", false}};
    fs.dirs["sdmc:/Music"].push_back({"Top Picks", true});
    fs.tags["sdmc:/Music/Top Picks/01 - One.mp3"] = tag("One", "C418", "Top Picks", 1, 100);
    Tags second = tag("Two", "Kate Bush", "Top Picks", 2, 200);
    second.albumArtist = "Various Artists";
    fs.tags["sdmc:/Music/Top Picks/01 - One.mp3"].albumArtist = "Various Artists";
    fs.tags["sdmc:/Music/Top Picks/02 - Two.mp3"] = second;
    Library lib = scanned(fs);
    bool found = false;
    for (const Album &a : lib.albums()) {
        if (a.title != "Top Picks") {
            CHECK(!a.isPlaylist);
            continue;
        }
        found = true;
        CHECK(a.isPlaylist);
        CHECK_EQ(a.artist, std::string("Various artists"));
        CHECK_EQ(a.tracks.size(), size_t(2));
    }
    CHECK(found);
}

TEST("a playlist mosaic skips songs whose covers are the same picture")
{
    FakeFs fs = sampleFs();
    fs.dirs["sdmc:/Music/Mix"] = {{"1.mp3", false}, {"2.mp3", false}, {"3.mp3", false}, {"4.mp3", false}};
    fs.dirs["sdmc:/Music"].push_back({"Mix", true});
    const char *artists[4] = {"A", "B", "C", "D"};
    for (int i = 0; i < 4; ++i) {
        Tags t = tag("Song " + std::to_string(i), artists[i], "Mix", i + 1, 60);
        t.albumArtist = "Various Artists";
        // Songs 1 and 2 share one picture; song 3 has another; song 4 has none.
        if (i < 2) t.picture.data = {1, 2, 3, 4};
        else if (i == 2) t.picture.data = {9, 9, 9};
        fs.tags["sdmc:/Music/Mix/" + std::to_string(i + 1) + ".mp3"] = t;
    }
    Library lib = scanned(fs);
    for (const Album &a : lib.albums()) {
        if (a.title != "Mix") continue;
        CHECK(a.isPlaylist);
        CHECK_EQ(a.mosaicTracks.size(), size_t(2));
        CHECK_EQ(lib.tracks()[a.mosaicTracks[0]].artist, std::string("A"));
        CHECK_EQ(lib.tracks()[a.mosaicTracks[1]].artist, std::string("C"));
    }
}

TEST("the index round-trips tracks and a rescan takes known files from it")
{
    FakeFs fs = sampleFs();
    Library first = scanned(fs);
    const std::string text = encodeIndex(first.tracks());
    std::vector<Track> known;
    CHECK(decodeIndex(text, known));
    CHECK_EQ(known.size(), first.tracks().size());
    for (size_t i = 0; i < known.size(); ++i) {
        CHECK_EQ(known[i].path, first.tracks()[i].path);
        CHECK_EQ(known[i].title, first.tracks()[i].title);
        CHECK_EQ(known[i].albumKey, first.tracks()[i].albumKey);
        CHECK_EQ(known[i].artHash, first.tracks()[i].artHash);
        CHECK_EQ(known[i].durationSeconds, first.tracks()[i].durationSeconds);
    }

    // A second scan with the index reads no tags for files it knows; a new
    // file still gets read, and a removed one disappears.
    fs.dirs["sdmc:/Music/Loose"] = {{"untagged tone.wav", false}, {"10 New.mp3", false}};
    fs.tags["sdmc:/Music/Loose/10 New.mp3"] = tag("Brand New", "Someone", "Loose", 10, 100);
    int reads = 0;
    TagReader counting = [&](const std::string &path, Format f, Tags &out) {
        ++reads;
        return fs.reader()(path, f, out);
    };
    Library second;
    second.collect("sdmc:/Music", fs.lister());
    second.useIndex(known);
    while (second.step(counting, 3)) {
    }
    second.build();
    CHECK_EQ(reads, 1);
    CHECK_EQ(second.reusedCount(), 4);
    CHECK_EQ(second.tracks().size(), size_t(5));
    bool sawNew = false;
    bool sawRemoved = false;
    for (const Track &t : second.tracks()) {
        if (t.title == "Brand New") sawNew = true;
        if (t.path.find("Another.opus") != std::string::npos) sawRemoved = true;
    }
    CHECK(sawNew);
    CHECK(!sawRemoved);
    // The reused album still groups and keeps its year and cover.
    const Album *pocket = nullptr;
    for (const Album &a : second.albums()) {
        if (a.title == "Pocket Park") pocket = &a;
    }
    CHECK(pocket != nullptr);
    CHECK_EQ(pocket->tracks.size(), size_t(3));
    CHECK_EQ(pocket->year, std::string("1980"));
    CHECK(!pocket->artPath.empty());
}

TEST("a corrupt index is rejected and a field with a tab survives")
{
    std::vector<Track> out;
    CHECK(!decodeIndex("not an index\n", out));
    Track t;
    t.path = "sdmc:/Music/A/1.mp3";
    t.title = "Tab\there";
    t.albumKey = "a|b";
    CHECK(decodeIndex(encodeIndex({t}), out));
    CHECK_EQ(out.size(), size_t(1));
    CHECK_EQ(out[0].title, std::string("Tab here"));
}

TEST("the library rebuilds itself from the index alone, covers included")
{
    FakeFs fs = sampleFs();
    Library first = scanned(fs);
    const std::string text = encodeIndex(first.tracks(), first.looseCovers());
    CHECK(!first.looseCovers().empty());

    std::vector<Track> tracks;
    std::vector<Cover> covers;
    CHECK(decodeIndex(text, tracks, covers));
    CHECK_EQ(covers.size(), first.looseCovers().size());

    Library fromIndex;
    fromIndex.loadFromIndex("sdmc:/Music", tracks, covers);
    CHECK_EQ(fromIndex.tracks().size(), first.tracks().size());
    CHECK_EQ(fromIndex.albums().size(), first.albums().size());
    CHECK(!fromIndex.scanning());
    for (size_t i = 0; i < first.albums().size(); ++i) {
        const Album &a = first.albums()[i];
        const Album &b = fromIndex.albums()[i];
        CHECK_EQ(b.title, a.title);
        CHECK_EQ(b.artist, a.artist);
        CHECK_EQ(b.folder, a.folder);
        CHECK_EQ(b.artPath, a.artPath);
        CHECK_EQ(b.artIsEmbedded, a.artIsEmbedded);
        CHECK_EQ(b.tracks.size(), a.tracks.size());
        CHECK_EQ(b.isPlaylist, a.isPlaylist);
    }
    // An unchanged library encodes to the same bytes, which is how the app
    // knows a background check found nothing new.
    CHECK_EQ(encodeIndex(fromIndex.tracks(), fromIndex.looseCovers()), text);
    // An index from an older scanner is refused, so its files are read again.
    std::string old = text;
    old.replace(0, std::string("3dsFM library index 3").size(), "3dsFM library index 2");
    CHECK(!decodeIndex(old, tracks, covers));
}

TEST("video-site noise comes off titles and file names split into artist and title")
{
    CHECK_EQ(cleanTitle("Gimme Chocolate!! (Official Music Video)"), std::string("Gimme Chocolate!!"));
    CHECK_EQ(cleanTitle("She Looks So Perfect [Official Video] [HD]"), std::string("She Looks So Perfect"));
    CHECK_EQ(cleanTitle("Doki Doki Morning - Official Audio"), std::string("Doki Doki Morning"));
    CHECK_EQ(cleanTitle("Never Gonna Give You Up [dQw4w9WgXcQ]"), std::string("Never Gonna Give You Up"));
    CHECK_EQ(cleanTitle("Not Tomorrow (Long Version)"), std::string("Not Tomorrow (Long Version)"));
    CHECK_EQ(cleanTitle("Promise (Reprise)"), std::string("Promise (Reprise)"));
    CHECK_EQ(cleanTitle("Song (feat. Someone) (Lyrics)"), std::string("Song (feat. Someone)"));
    std::string artist;
    std::string title;
    CHECK(splitArtistTitle("BABYMETAL - Doki Doki Morning", artist, title));
    CHECK_EQ(artist, std::string("BABYMETAL"));
    CHECK_EQ(title, std::string("Doki Doki Morning"));
    CHECK(splitArtistTitle("Miki Matsubara \xe2\x80\x93 Stay With Me", artist, title));
    CHECK_EQ(artist, std::string("Miki Matsubara"));
    CHECK(!splitArtistTitle("My War Full English Cover By_ Riverdude", artist, title));
    CHECK(isJunkFile("._Song.mp3"));
    CHECK(isJunkFile(".DS_Store"));
    CHECK(isJunkFile("Thumbs.db"));
    CHECK(!isJunkFile("Song.mp3"));
}

TEST("loose songs in the music folder become per-artist singles and junk is listed")
{
    FakeFs fs;
    fs.dirs["sdmc:/Music"] = {
        {"5 Seconds of Summer - She Looks So Perfect (Official Video).mp3", false},
        {"._5 Seconds of Summer - She Looks So Perfect (Official Video).mp3", false},
        {"BABYMETAL - Gimme Chocolate [dQw4w9WgXcQ].mp3", false},
        {"BABYMETAL - Megitsune.mp3", false},
        {"My War Full English Cover By_ Riverdude.mp3", false},
        {".DS_Store", false},
        {"Miki Matsubara", true},
    };
    fs.dirs["sdmc:/Music/Miki Matsubara"] = {{"Pocket Park", true}};
    fs.dirs["sdmc:/Music/Miki Matsubara/Pocket Park"] = {{"01 - Mayonaka no Door.mp3", false}};
    Tags gimme = tag("Gimme Chocolate!! (Official Music Video)", "BABYMETAL", "", 0, 200);
    gimme.picture.data = {1, 2, 3};
    fs.tags["sdmc:/Music/BABYMETAL - Gimme Chocolate [dQw4w9WgXcQ].mp3"] = gimme;
    Tags megitsune = tag("Megitsune", "BABYMETAL", "BABYMETAL", 1, 210);
    megitsune.picture.data = {4, 5, 6};
    fs.tags["sdmc:/Music/BABYMETAL - Megitsune.mp3"] = megitsune;
    fs.tags["sdmc:/Music/Miki Matsubara/Pocket Park/01 - Mayonaka no Door.mp3"] = tag("Mayonaka no Door", "Miki Matsubara", "Pocket Park", 1, 257, "1980");

    Library lib;
    lib.collect("sdmc:/Music", fs.lister());
    CHECK_EQ(lib.collectedCount(), 5);
    CHECK_EQ(lib.junkFiles().size(), size_t(2));
    while (lib.step(fs.reader(), 2)) {
    }
    lib.build();
    CHECK_EQ(lib.tracks().size(), size_t(5));

    std::map<std::string, const Album *> byTitleArtist;
    for (const Album &a : lib.albums()) byTitleArtist[a.title + "|" + a.artist] = &a;
    CHECK_EQ(lib.albums().size(), size_t(4));
    const Album *babymetal = byTitleArtist["Singles|BABYMETAL"];
    CHECK(babymetal != nullptr);
    if (babymetal != nullptr) {
        CHECK(babymetal->isSingles);
        CHECK(!babymetal->isPlaylist);
        CHECK_EQ(babymetal->tracks.size(), size_t(2));
        CHECK_EQ(babymetal->mosaicTracks.size(), size_t(2));
        CHECK(babymetal->artIsEmbedded);
        const Track *g = nullptr;
        for (int t : babymetal->tracks) if (lib.tracks()[t].path.find("Gimme") != std::string::npos) g = &lib.tracks()[t];
        CHECK(g != nullptr);
        if (g != nullptr) {
            CHECK_EQ(g->title, std::string("Gimme Chocolate!!"));
            CHECK_EQ(g->album, std::string("Gimme Chocolate!!"));
        }
    }
    const Album *fiveSos = byTitleArtist["Singles|5 Seconds of Summer"];
    CHECK(fiveSos != nullptr);
    if (fiveSos != nullptr) {
        CHECK_EQ(lib.tracks()[fiveSos->tracks[0]].title, std::string("She Looks So Perfect"));
        CHECK_EQ(lib.tracks()[fiveSos->tracks[0]].artist, std::string("5 Seconds of Summer"));
    }
    const Album *unknown = byTitleArtist["Singles|Unknown artist"];
    CHECK(unknown != nullptr);
    if (unknown != nullptr) CHECK_EQ(lib.tracks()[unknown->tracks[0]].title, std::string("My War Full English Cover By_ Riverdude"));
    const Album *pocket = byTitleArtist["Pocket Park|Miki Matsubara"];
    CHECK(pocket != nullptr);
    if (pocket != nullptr) CHECK(!pocket->isSingles);

    // The artists list has one entry per artist and singles sort after albums.
    const std::vector<std::string> artists = lib.artists();
    CHECK_EQ(artists.size(), size_t(4));

    // Reloading from the index gives the same collections.
    std::vector<Track> tracks;
    std::vector<Cover> covers;
    CHECK(decodeIndex(encodeIndex(lib.tracks(), lib.looseCovers()), tracks, covers));
    Library again;
    again.loadFromIndex("sdmc:/Music", tracks, covers);
    CHECK_EQ(again.albums().size(), lib.albums().size());
    for (size_t i = 0; i < lib.albums().size(); ++i) {
        CHECK_EQ(again.albums()[i].key, lib.albums()[i].key);
        CHECK_EQ(again.albums()[i].title, lib.albums()[i].title);
        CHECK_EQ(again.albums()[i].tracks.size(), lib.albums()[i].tracks.size());
    }
    CHECK_EQ(encodeIndex(again.tracks(), again.looseCovers()), encodeIndex(lib.tracks(), lib.looseCovers()));
}
