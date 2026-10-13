#include "app.hpp"

#include "art.hpp"
#include "audio.hpp"
#include "library.hpp"
#include "platform.hpp"
#include "queue.hpp"
#include "time_format.hpp"
#include "ui.hpp"

#include <3ds.h>
#include <citro2d.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <functional>
#include <unordered_map>
#include <map>
#include <string>
#include <vector>

using ui::Icon;

namespace {

constexpr float TopW = 400.0f;
constexpr float TopH = 240.0f;
constexpr float BotW = 320.0f;
constexpr float BotH = 240.0f;

// Bottom screen layout.
// Sizes on the bottom screen are generous: on a console the 2x mockups'
// values came out too small to hit with a thumb or read at arm's length.
constexpr float TabY = 17.0f;
constexpr float TabX0 = 24.0f;
constexpr float TabStep = 32.0f;
constexpr float TabIcon = 18.0f;
constexpr float RoundY = 17.0f;
constexpr float RoundX0 = 244.0f;
constexpr float RoundStep = 28.0f;
constexpr float RoundR = 12.0f;
constexpr float GridX = 41.0f;
constexpr float GridY = 36.0f;
constexpr float Tile = 74.0f;
constexpr float TileStep = 82.0f;
constexpr int GridCols = 3;
constexpr int GridRows = 2;
// The fixed details strip under the grid on the Home section.
constexpr float StripX = 10.0f;
constexpr float StripY = 194.0f;
constexpr float StripW = 300.0f;
constexpr float StripH = 42.0f;
constexpr float RowH = 37.0f;       // five rows fit under the tabs
constexpr float RowStep = 40.0f;
constexpr float RowX = 10.0f;
constexpr float RowW = 300.0f;
constexpr int SectionRows = 5;      // rows visible under the tabs
constexpr float HeaderY = 34.0f;    // the album and artist header card sits under the tabs
constexpr float HeaderH = 40.0f;
constexpr float HeaderRowsY = 80.0f;
constexpr int HeaderRows = 4;
constexpr float MenuRowH = 31.0f;   // menus keep tighter rows so six items fit
constexpr float MenuRowStep = 34.0f;
constexpr float NowButtonsY = 54.0f;  // Now Playing: the top screen shows the song, so the
constexpr float NowSeekY = 80.0f;     // bottom has only the transport, a seek bar and the queue
constexpr float NowQueueLabelY = 90.0f;
constexpr float NowQueueY = 104.0f;
constexpr int NowQueueRows = 3;
constexpr float SearchBarH = 30.0f;
constexpr float SearchRowsY = 48.0f;
constexpr int SearchRows = 4;
constexpr float SettingHeaderH = 15.0f;
constexpr float SettingRowH = 23.0f;
constexpr float MenuX = 48.0f;
constexpr float MenuY = 14.0f;
constexpr float MenuW = 170.0f;
constexpr float DialogButtonY = 146.0f;
constexpr float DialogButtonH = 24.0f;

enum class View { Section, Album, Artist, NowPlaying, Search, Settings, Visualizer };
enum class Section { Home, Songs, Artists, UpNext };

struct MenuItem {
    std::string label;
    Icon icon;
    bool enabled;
    std::function<void()> action;
};

struct Row {
    std::string title;
    std::string sub;
    std::string right;
    int album = -1;      // for the thumbnail
    int track = -1;      // preferred over album for the thumbnail
    bool playing = false;
    bool dim = false;
};

u32 keysDownRepeat()
{
    static int held = 0;
    static u32 last = 0;
    const u32 down = hidKeysDown();
    const u32 heldKeys = hidKeysHeld() & (KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT | KEY_CPAD_UP | KEY_CPAD_DOWN | KEY_CPAD_LEFT | KEY_CPAD_RIGHT);
    if (heldKeys == 0 || heldKeys != last) {
        held = 0;
        last = heldKeys;
        return down;
    }
    ++held;
    if (held > 18 && (held - 18) % 5 == 0) return down | heldKeys;
    return down;
}

bool up(u32 k) { return k & (KEY_DUP | KEY_CPAD_UP); }
bool down(u32 k) { return k & (KEY_DDOWN | KEY_CPAD_DOWN); }
bool left(u32 k) { return k & (KEY_DLEFT | KEY_CPAD_LEFT); }
bool right(u32 k) { return k & (KEY_DRIGHT | KEY_CPAD_RIGHT); }

std::string plural(int n, const char *one, const char *many)
{
    return std::to_string(n) + " " + (n == 1 ? one : many);
}

std::string minutesText(int seconds)
{
    const int minutes = (seconds + 30) / 60;
    if (minutes >= 60) return std::to_string(minutes / 60) + " h " + std::to_string(minutes % 60) + " min";
    return std::to_string(minutes) + " min";
}

std::string gigabytes(int64_t bytes)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.1f GB", double(bytes) / (1024.0 * 1024.0 * 1024.0));
    return buffer;
}

} // namespace

struct App::State {
    platform::Settings settings;
    core::Library library;
    core::PlayQueue queue;
    audio::Player player;
    bool audioReady = false;
    std::string audioError;

    View view = View::Section;
    View previous = View::Section;
    Section section = Section::Home;
    int focusHome = 0;
    int focusSongs = 0;
    int focusArtists = 0;
    int focusUpNext = 0;
    int focusAlbum = 0;
    int focusArtist = 0;
    int focusSearch = 0;
    int focusSettings = 2;
    int focusNow = 2; // transport index
    int focusNowQueue = -1; // selected row in the Now Playing up-next list, -1 none
    int focusViz = 0;
    int scrollRows = 0;
    int listScroll = 0;
    int settingsScroll = 0; // the settings sheet scrolls by the pixel
    // Pixel positions of the two scrollers; the integers above are derived
    // from these so keyboard focus logic keeps working after a drag.
    float gridPx = 0.0f;
    float listPx = 0.0f;

    int albumView = -1;
    std::string artistView;
    std::vector<int> artistAlbums;
    std::string searchQuery;
    std::vector<int> searchResults;
    int nowTrack = -1;
    bool scanning = true;
    bool emptyDialogShown = false;
    uint32_t frame = 0;
    int visualizerDim = 0;

    // Touch drag scrolling. A press records where the list was; moving more
    // than a few pixels turns the press into a drag, and letting go with
    // speed flicks the list on until it settles on a row boundary.
    struct Drag {
        bool held = false;
        bool moved = false;
        float startY = 0.0f;
        float startPx = 0.0f;
        float px = 0.0f;
        float lastY = 0.0f;
        float velocity = 0.0f;
        bool coasting = false;
        touchPosition pressAt{};
    } drag;

    struct ScrollInfo {
        int *index = nullptr; // the row-index scroll the keyboard uses
        float step = 0.0f;
        int maxIndex = 0;
    };
    float &pxFor(const ScrollInfo &info)
    {
        if (info.index == &scrollRows) return gridPx;
        if (info.index == &settingsScroll) return settingsScrollPx;
        return listPx;
    }

    ScrollInfo currentScroll()
    {
        ScrollInfo info;
        if (menuOpen || dialogOpen) return info;
        switch (view) {
        case View::Section:
            if (section == Section::Home) {
                const int rows = (int(library.albums().size()) + GridCols - 1) / GridCols;
                info = {&scrollRows, TileStep, std::max(0, rows - GridRows)};
            } else if (section == Section::Songs) {
                info = {&listScroll, RowStep, std::max(0, int(allTracks().size()) - SectionRows)};
            } else if (section == Section::Artists) {
                info = {&listScroll, RowStep, std::max(0, int(library.artists().size()) - SectionRows)};
            } else if (section == Section::UpNext) {
                info = {&listScroll, RowStep, std::max(0, int(queue.upcoming().size()) - SectionRows)};
            }
            break;
        case View::Album:
            if (albumView >= 0) info = {&listScroll, RowStep, std::max(0, int(library.albums()[albumView].tracks.size()) - HeaderRows)};
            break;
        case View::Artist:
            info = {&listScroll, RowStep, std::max(0, int(artistAlbums.size()) - HeaderRows)};
            break;
        case View::Search:
            info = {&listScroll, RowStep, std::max(0, int(searchResults.size()) - SearchRows)};
            break;
        case View::Settings:
            info = {&settingsScroll, 1.0f, std::max(0, int(settingsHeight(settingRows()) + 8.0f - BotH))};
            break;
        default:
            break;
        }
        return info;
    }

    // Pixel offset of the current list, fractional while dragging.
    float scrollPx()
    {
        const ScrollInfo info = currentScroll();
        if (info.index == nullptr) return 0.0f;
        float &px = pxFor(info);
        const float maxPx = info.maxIndex * info.step;
        if (px > maxPx) px = maxPx;
        if (px < 0.0f) px = 0.0f;
        return px;
    }

    // Returns true when a tap should be delivered this frame.
    bool updateDrag(bool &tapped, touchPosition &touch)
    {
        const u32 held = hidKeysHeld();
        const u32 up = hidKeysUp();
        touchPosition now{};
        hidTouchRead(&now);
        const ScrollInfo info = currentScroll();
        tapped = false;
        if ((hidKeysDown() & KEY_TOUCH) != 0) {
            drag.held = true;
            drag.moved = false;
            drag.pressAt = now;
            drag.startY = now.py;
            drag.lastY = now.py;
            drag.velocity = 0.0f;
            drag.startPx = info.index != nullptr ? scrollPx() : 0.0f;
            drag.px = drag.startPx;
            drag.coasting = false;
        } else if (drag.held && (held & KEY_TOUCH) != 0) {
            if (info.index != nullptr) {
                const float dy = drag.startY - float(now.py);
                if (!drag.moved && std::fabs(dy) > 6.0f) drag.moved = true;
                if (drag.moved) {
                    const float maxPx = info.maxIndex * info.step;
                    drag.px = std::min(maxPx, std::max(0.0f, drag.startPx + dy));
                    drag.velocity = float(drag.lastY) - float(now.py);
                    pxFor(info) = drag.px;
                }
            }
            drag.lastY = now.py;
        } else if (drag.held && (up & KEY_TOUCH) != 0) {
            drag.held = false;
            if (!drag.moved) {
                tapped = true;
                touch = drag.pressAt;
            } else if (info.index != nullptr) {
                drag.coasting = true;
            }
        }
        if (drag.coasting && !drag.held) {
            if (info.index == nullptr) {
                drag.coasting = false;
            } else {
                const float maxPx = info.maxIndex * info.step;
                drag.px = std::min(maxPx, std::max(0.0f, drag.px + drag.velocity));
                pxFor(info) = drag.px;
                drag.velocity *= 0.88f;
                if (std::fabs(drag.velocity) < 0.2f || drag.px <= 0.0f || drag.px >= maxPx) {
                    // Stop where the flick ran out; no snapping to a row.
                    drag.coasting = false;
                    drag.moved = false;
                    drag.velocity = 0.0f;
                }
            }
        }
        if (info.index != nullptr) {
            // The integer scroll follows the pixels so key focus stays sane.
            *info.index = std::max(0, std::min(info.maxIndex, int(pxFor(info) / info.step)));
        }
        return tapped;
    }

    // Overlays.
    bool menuOpen = false;
    std::vector<MenuItem> menu;
    int menuFocus = 0;
    bool dialogOpen = false;
    std::string dialogTitle;
    std::string dialogBody;
    std::string dialogPrimary;
    std::string dialogSecondary;
    std::function<void(bool)> dialogClose;
    std::string toastText;
    int toastFrames = 0;

    std::map<std::string, art::Image> art;
    bool artLoadedThisFrame = false;
    float bands[20] = {};

    // Console status, refreshed once a second: each of these is a service
    // call and the top screen used to make all of them every frame.
    struct Status {
        std::string clock;
        int battery = 5;
        bool charging = false;
        int wifi = 0;
        bool headphones = false;
    } status;
    void refreshStatus()
    {
        status.clock = platform::clockText() + "  |  " + platform::dateText();
        status.battery = platform::batteryLevel();
        status.charging = platform::charging();
        status.wifi = platform::wifiStrength();
        status.headphones = audioReady && platform::headphonesPlugged();
    }
    // The showcase title's fitted size, remembered per title.
    std::string fitTitle;
    float fitResult = 30.0f;
    float fittedShowcaseSize(const std::string &title)
    {
        if (title != fitTitle) {
            fitTitle = title;
            fitResult = ui::fitSize(title, 30.0f, 24.0f, textMaxW());
        }
        return fitResult;
    }
    std::vector<int> allTracksCache;
    bool allTracksDirty = true;

    // Scanning runs on its own thread with its own Library; the main thread
    // swaps it in when it finishes. The thread lists the folder, takes every
    // file it already knows from the index on the SD card, reads tags only
    // for new files, and writes the index back as it goes, so a launch after
    // the first reads no music files at all.
    core::Library *scanLibrary = nullptr;
    Thread scanThread = nullptr;
    volatile bool scanDone = false;
    // What the index on the card knew when the scan started. The library is
    // built from it straight away; the thread uses it to skip known files.
    std::vector<core::Track> scanKnown;
    std::vector<core::Cover> scanKnownCovers;
    bool libraryFromIndex = false;
    std::atomic<bool> scanPaused{false}; // while the HOME Menu or sleep has the console
    std::atomic<bool> scanAbort{false};
    std::atomic<int> scanProgress{0};
    std::atomic<int> scanTotal{0};
    bool scanFull = false; // Settings > Rescan: read every file again
    static void scanEntry(void *arg)
    {
        State *self = static_cast<State *>(arg);
        core::Library &lib = *self->scanLibrary;
        lib.collect(self->settings.musicFolder, platform::listDirectory);
        self->scanTotal = lib.collectedCount();
        platform::log("scan: %d files under %s", lib.collectedCount(), self->settings.musicFolder.c_str());
        if (!self->scanFull && !self->scanKnown.empty()) lib.useIndex(self->scanKnown);
        int fresh = 0; // tags read since the index was last written
        int reused = 0;
        bool more = true;
        while (more && !self->scanAbort) {
            if (self->scanPaused) {
                svcSleepThread(50 * 1000 * 1000);
                continue;
            }
            more = lib.step(platform::readTagsFromFile, 1);
            self->scanProgress = lib.scannedCount();
            if (lib.reusedCount() == reused) ++fresh; else reused = lib.reusedCount();
            if (fresh >= 20) {
                platform::writeTextFile(platform::IndexPath, core::encodeIndex(lib.tracks(), lib.looseCovers()));
                fresh = 0;
            }
        }
        if (!self->scanAbort) {
            lib.build();
            const bool saved = platform::writeTextFile(platform::IndexPath, core::encodeIndex(lib.tracks(), lib.looseCovers()));
            platform::log("scan: %d tracks, %d from the index, index %s", int(lib.tracks().size()), lib.reusedCount(), saved ? "saved" : "NOT saved");
        }
        self->scanDone = true;
    }

    // The HOME Menu and sleep: stop the scan thread and the music while
    // the console is away, start them again when it comes back. A thread
    // that keeps working while the app is suspended can starve the HOME
    // Menu, and the DSP is handed over anyway.
    // Playback is stopped outright, not paused: the decode thread is joined
    // and the channel reset, so the DSP goes to sleep with nothing queued
    // and no second-core thread touching it. The song picks up where it was
    // on the next frame after the console comes back.
    aptHookCookie aptCookie{};
    bool resumeAfterSystem = false;
    double resumePosition = 0.0;
    volatile bool resumePending = false;
    volatile bool handoverActive = false;
    u64 handoverStarted = 0;
    static void aptHookEntry(APT_HookType hook, void *param)
    {
        State *self = static_cast<State *>(param);
        switch (hook) {
        case APTHOOK_ONSUSPEND:
        case APTHOOK_ONSLEEP:
            platform::log("apt: %s (playing %d, scanning %d)", hook == APTHOOK_ONSUSPEND ? "suspend" : "sleep", self->player.playing() ? 1 : 0, self->scanning ? 1 : 0);
            self->scanPaused = true;
            art::setPaused(true);
            self->resumeAfterSystem = self->player.playing();
            self->resumePosition = self->player.positionSeconds();
            self->player.stop();
            self->handoverStarted = osGetTime();
            self->handoverActive = true;
            platform::log("apt: playback stopped at %.1f s, handing the console over", self->resumePosition);
            break;
        case APTHOOK_ONRESTORE:
        case APTHOOK_ONWAKEUP:
            self->handoverActive = false;
            platform::log("apt: %s after %llu ms", hook == APTHOOK_ONRESTORE ? "restore" : "wakeup", (unsigned long long)(osGetTime() - self->handoverStarted));
            self->scanPaused = false;
            art::setPaused(false);
            if (self->resumeAfterSystem) self->resumePending = true;
            break;
        default:
            break;
        }
    }

    // Picks the song up again once the DSP is back (libctru wakes it after
    // the restore hook, so this runs from the next frame, not the hook).
    void resumeAfterHandover()
    {
        if (!resumePending) return;
        resumePending = false;
        resumeAfterSystem = false;
        // The system hands the app's share of the second core back as zero
        // after a suspend, which leaves a decode thread there with no CPU
        // at all, so the share is asked for again before playback restarts.
        u32 share = 0;
        APT_GetAppCpuTimeLimit(&share);
        const Result set = APT_SetAppCpuTimeLimit(30);
        u32 after = 0;
        APT_GetAppCpuTimeLimit(&after);
        platform::log("apt: second-core share %lu%% on return, set to %lu%% (%08lx)", (unsigned long)share, (unsigned long)after, (unsigned long)set);
        if (nowTrack < 0 || nowTrack >= int(library.tracks().size())) return;
        const core::Track &t = library.tracks()[nowTrack];
        std::string error;
        if (!player.play(t.path, t.format, error)) {
            platform::log("apt: could not resume %s (%s)", t.path.c_str(), error.c_str());
            return;
        }
        if (resumePosition > 1.0) player.seek(resumePosition);
        platform::log("apt: resumed %s at %.1f s", t.path.c_str(), resumePosition);
    }

    // A low-priority thread that notes, every two seconds, that the app is
    // still waiting for the system to take or give back the console. If a
    // freeze leaves these lines behind, the process and the SD card were
    // alive and the main thread was stuck in the hand-over; if it leaves
    // none, the whole system stopped.
    Thread watchdogThread = nullptr;
    volatile bool watchdogRunning = false;
    static void watchdogEntry(void *arg)
    {
        State *self = static_cast<State *>(arg);
        u64 lastNoted = 0;
        while (self->watchdogRunning) {
            svcSleepThread(500 * 1000 * 1000);
            if (!self->handoverActive) {
                lastNoted = 0;
                continue;
            }
            const u64 elapsed = osGetTime() - self->handoverStarted;
            if (elapsed >= 2000 && elapsed - lastNoted >= 2000) {
                lastNoted = elapsed;
                platform::log("apt: still handing over after %llu ms", (unsigned long long)elapsed);
            }
        }
    }
    void startWatchdog()
    {
        watchdogRunning = true;
        s32 prio = 0;
        svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
        watchdogThread = threadCreate(watchdogEntry, this, 16 * 1024, prio + 2, -2, false);
        if (watchdogThread == nullptr) watchdogRunning = false;
    }
    void stopWatchdog()
    {
        if (watchdogThread == nullptr) return;
        watchdogRunning = false;
        threadJoin(watchdogThread, U64_MAX);
        threadFree(watchdogThread);
        watchdogThread = nullptr;
    }

    // A scripted input walk for crash testing in the emulator: when
    // sdmc:/3ds/3dsFM/smoke.txt exists, its space-separated key names are
    // pressed one every 20 frames and the view is logged after each.
    std::vector<u32> smoke;
    size_t smokeIndex = 0;
    bool smokeLoaded = false;

    u32 smokeKey()
    {
        if (!smokeLoaded) {
            smokeLoaded = true;
            std::vector<uint8_t> bytes;
            if (platform::readWholeFile("sdmc:/3ds/3dsFM/smoke.txt", bytes, 65536)) {
                std::string token;
                auto flush = [&] {
                    static const struct { const char *name; u32 key; } names[] = {
                        {"A", KEY_A}, {"B", KEY_B}, {"X", KEY_X}, {"Y", KEY_Y}, {"L", KEY_L}, {"R", KEY_R},
                        {"UP", KEY_DUP}, {"DOWN", KEY_DDOWN}, {"LEFT", KEY_DLEFT}, {"RIGHT", KEY_DRIGHT},
                        {"START", KEY_START}, {"SELECT", KEY_SELECT}, {"WAIT", 0}};
                    for (const auto &n : names) if (token == n.name) smoke.push_back(n.key);
                    token.clear();
                };
                for (uint8_t c : bytes) {
                    if (c == ' ' || c == '\n' || c == '\r' || c == '\t') flush(); else token += char(c);
                }
                flush();
                platform::log("smoke: %d steps", int(smoke.size()));
            }
        }
        if (smokeIndex >= smoke.size() || scanning) return 0;
        if (frame % 20 != 0) return 0;
        const u32 key = smoke[smokeIndex++];
        platform::log("smoke %d: key 0x%lx view=%d section=%d focusHome=%d menu=%d dialog=%d now=%d playing=%d", int(smokeIndex), (unsigned long)key, int(view), int(section), focusHome, menuOpen ? 1 : 0, dialogOpen ? 1 : 0, nowTrack, player.playing() ? 1 : 0);
        return key;
    }

    // ---- helpers ----
    void toast(const std::string &text)
    {
        toastText = text;
        toastFrames = 150;
    }

    void dialog(const std::string &title, const std::string &body, const std::string &primary, const std::string &secondary, std::function<void(bool)> onClose)
    {
        dialogOpen = true;
        dialogTitle = title;
        dialogBody = body;
        dialogPrimary = primary;
        dialogSecondary = secondary;
        dialogClose = std::move(onClose);
    }

    void closeDialog(bool primary)
    {
        dialogOpen = false;
        if (dialogClose) dialogClose(primary);
    }

    void go(View v)
    {
        if (v != view) {
            previous = view;
            view = v;
        }
        if (!chromeShown()) chromeFocus = -1;
    }

    // The cursor can climb into the top row of the bottom screen: the four
    // tabs, then Search, Now Playing and Settings (0 to 6). -1 means it is
    // down in the content. Search and Settings draw no top row, so the
    // cursor lands in their content when they open.
    int chromeFocus = -1;
    bool chromeShown() const { return view == View::Section || view == View::NowPlaying || view == View::Album || view == View::Artist; }
    bool chromeHasTabs() const { return view == View::Section || view == View::NowPlaying || view == View::Album || view == View::Artist; }
    static constexpr int ChromeSearch = 4;
    static constexpr int ChromeNow = 5;
    static constexpr int ChromeSettings = 6;
    void enterChrome() { chromeFocus = view == View::Section || view == View::Album || view == View::Artist ? int(section) : view == View::NowPlaying ? ChromeNow : ChromeSearch; }
    void activateChrome(int i)
    {
        if (i < ChromeSearch) {
            section = Section(i);
            listScroll = 0;
            listPx = 0.0f;
            if (view != View::Section) go(View::Section);
        } else if (i == ChromeSearch) {
            go(View::Search);
        } else if (i == ChromeNow) {
            go(View::NowPlaying);
        } else {
            go(View::Settings);
        }
    }
    // Keys while the cursor is in the top row. Returns false when it is not.
    bool updateChrome(u32 k, u32 kd)
    {
        if (chromeFocus < 0) return false;
        const int first = chromeHasTabs() ? 0 : ChromeSearch;
        if (chromeFocus < first) chromeFocus = first;
        if (left(k)) chromeFocus = chromeFocus > first ? chromeFocus - 1 : ChromeSettings;
        if (right(k)) chromeFocus = chromeFocus < ChromeSettings ? chromeFocus + 1 : first;
        if (down(k) || (kd & KEY_B)) {
            chromeFocus = -1;
            return true;
        }
        if (kd & KEY_A) activateChrome(chromeFocus);
        return true;
    }

    void back()
    {
        switch (view) {
        case View::Album:
            view = artistView.empty() ? View::Section : View::Artist;
            if (view == View::Section && section == Section::Artists) view = View::Artist;
            if (artistView.empty()) view = View::Section;
            break;
        case View::Artist:
            artistView.clear();
            view = View::Section;
            section = Section::Artists;
            break;
        case View::NowPlaying:
        case View::Search:
        case View::Settings:
            view = previous == view ? View::Section : previous;
            if (view == View::Visualizer) view = View::NowPlaying;
            break;
        case View::Visualizer:
            view = View::NowPlaying;
            break;
        default:
            break;
        }
    }

    // ---- library and scanning ----
    void finishScanThread()
    {
        if (scanThread != nullptr) {
            scanAbort = true;
            scanPaused = false;
            threadJoin(scanThread, U64_MAX);
            threadFree(scanThread);
            scanThread = nullptr;
        }
        delete scanLibrary;
        scanLibrary = nullptr;
    }

    // Starts with whatever the index on the card knows, so the library is
    // there to browse and play at once, then checks the folder for changes
    // on the scan thread. A full scan ignores the index and reads every file.
    void startScan(bool full = false)
    {
        finishScanThread();
        scanKnown.clear();
        scanKnownCovers.clear();
        libraryFromIndex = false;
        if (!full) {
            std::string text;
            const bool found = platform::readTextFile(platform::IndexPath, text) || platform::readTextFile(std::string(platform::IndexPath) + ".tmp", text);
            if (!found) {
                platform::log("index: none at %s", platform::IndexPath);
            } else if (!core::decodeIndex(text, scanKnown, scanKnownCovers) || scanKnown.empty()) {
                platform::log("index: unreadable or empty (%d bytes)", int(text.size()));
                scanKnown.clear();
                scanKnownCovers.clear();
            } else {
                library.loadFromIndex(scanKnown, scanKnownCovers);
                libraryFromIndex = true;
                platform::log("index: %d tracks, %d albums, %d covers loaded", int(library.tracks().size()), int(library.albums().size()), int(scanKnownCovers.size()));
            }
        }
        if (!libraryFromIndex) {
            library.clear();
            for (auto &entry : art) art::release(entry.second);
            art.clear();
        }
        allTracksDirty = true;
        emptyDialogShown = false;
        scanLibrary = new core::Library;
        scanFull = full;
        scanDone = false;
        scanAbort = false;
        scanPaused = false;
        scanProgress = 0;
        scanTotal = 0;
        scanning = true;
        // Two steps below the UI thread, under libctru's APT handler thread,
        // so the HOME button is never waiting on a tag read.
        s32 prio = 0;
        svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
        scanThread = threadCreate(scanEntry, this, 128 * 1024, prio + 2, -2, false);
        if (scanThread == nullptr) scanThread = threadCreate(scanEntry, this, 64 * 1024, prio + 2, -2, false);
        if (scanThread == nullptr) {
            platform::log("scan: could not start the scan thread");
            delete scanLibrary;
            scanLibrary = nullptr;
            scanning = false;
            toast("Could not start the music scan");
        }
    }

    int scanCollected() const { return scanTotal; }
    int scanScanned() const { return scanProgress; }

    void stepScan()
    {
        if (!scanning) return;
        if (!scanDone) return;
        core::Library &fresh = *scanLibrary;
        const bool unchanged = libraryFromIndex && core::encodeIndex(fresh.tracks(), fresh.looseCovers()) == core::encodeIndex(library.tracks(), library.looseCovers());
        if (unchanged) {
            platform::log("scan done: nothing changed, %d tracks", int(library.tracks().size()));
        } else {
            // Take the new library, then point the queue and the current
            // song at their tracks by path; a song whose file is gone drops
            // out of the queue (its playback carries on to the end).
            const std::string nowPath = nowTrack >= 0 && nowTrack < int(library.tracks().size()) ? library.tracks()[nowTrack].path : "";
            library = std::move(fresh);
            std::unordered_map<std::string, int> byPath;
            for (int i = 0; i < int(library.tracks().size()); ++i) byPath[library.tracks()[i].path] = i;
            queue.remap([&](const core::Item &item) {
                const auto it = byPath.find(item.id);
                return it == byPath.end() ? -1 : it->second;
            });
            const auto now = byPath.find(nowPath);
            nowTrack = now == byPath.end() ? -1 : now->second;
            allTracksDirty = true;
            platform::log("scan done: %d tracks, %d albums", int(library.tracks().size()), int(library.albums().size()));
        }
        finishScanThread();
        scanning = false;
        libraryFromIndex = false;
        if (library.tracks().empty() && !emptyDialogShown) {
            emptyDialogShown = true;
            dialog("No music found", "Put folders of MP3, Ogg, Opus or WAV files under " + settings.musicFolder + " on the SD card, then rescan.", "Rescan", "OK", [this](bool primary) {
                if (primary) startScan();
            });
        }
    }

    bool debugNoArt = platform::fileExists("sdmc:/3ds/3dsFM/debug-noart");

    const art::Image *albumArt(int albumIndex)
    {
        if (debugNoArt) return nullptr;
        if (albumIndex < 0 || albumIndex >= int(library.albums().size())) return nullptr;
        const core::Album &album = library.albums()[albumIndex];
        if (album.artPath.empty()) return nullptr;
        auto it = art.find(album.key);
        if (it != art.end()) {
            it->second.lastUsedFrame = frame;
            return it->second.ready ? &it->second : nullptr;
        }
        if (art.size() >= 32) {
            auto oldest = art.begin();
            for (auto i = art.begin(); i != art.end(); ++i) {
                if (i->second.lastUsedFrame < oldest->second.lastUsedFrame) oldest = i;
            }
            art::release(oldest->second);
            art.erase(oldest);
        }
        // Placeholder entry until the loader thread delivers the pixels.
        art::Image &image = art[album.key];
        image.lastUsedFrame = frame;
        art::request(album.key, album.artPath, album.artIsEmbedded);
        return nullptr;
    }

    // Art for a specific track: its own embedded picture when it has one
    // (compilations have a different cover per song), else the album's.
    const art::Image *trackArt(int trackIndex)
    {
        if (debugNoArt) return nullptr;
        if (trackIndex < 0 || trackIndex >= int(library.tracks().size())) return nullptr;
        const core::Track &t = library.tracks()[trackIndex];
        if (!t.hasEmbeddedArt) return albumArt(albumIndexOfTrack(trackIndex));
        const std::string key = "track:" + t.path;
        auto it = art.find(key);
        if (it != art.end()) {
            it->second.lastUsedFrame = frame;
            return it->second.ready ? &it->second : nullptr;
        }
        if (art.size() >= 32) {
            auto oldest = art.begin();
            for (auto i = art.begin(); i != art.end(); ++i) {
                if (i->second.lastUsedFrame < oldest->second.lastUsedFrame) oldest = i;
            }
            art::release(oldest->second);
            art.erase(oldest);
        }
        art::Image &image = art[key];
        image.lastUsedFrame = frame;
        art::request(key, t.path, true);
        return nullptr;
    }

    // One finished decode becomes a texture per frame.
    void pollArt()
    {
        std::string key;
        art::Image image;
        if (!art::takeFinished(key, image)) return;
        auto it = art.find(key);
        if (it == art.end()) {
            art::release(image);
            return;
        }
        const uint32_t used = it->second.lastUsedFrame;
        it->second = image;
        it->second.lastUsedFrame = used;
        if (image.failed) platform::log("art failed: %s", key.c_str());
    }

    int albumIndexOfTrack(int trackIndex) const
    {
        if (trackIndex < 0 || trackIndex >= int(library.tracks().size())) return -1;
        return library.albumIndex(library.tracks()[trackIndex].albumKey);
    }

    // If a song is meant to be playing but its position has not moved for
    // three seconds, the decode thread is getting no CPU (the second core
    // after a HOME Menu visit, typically). Decoding moves to the app core
    // and the song restarts where it was.
    double stallPosition = -1.0;
    int stallFrames = 0;
    void watchForStall()
    {
        if (!player.playing()) {
            stallFrames = 0;
            stallPosition = -1.0;
            return;
        }
        const double pos = player.positionSeconds();
        if (pos != stallPosition) {
            stallPosition = pos;
            stallFrames = 0;
            return;
        }
        if (++stallFrames < 180) return;
        stallFrames = 0;
        if (nowTrack < 0 || nowTrack >= int(library.tracks().size())) return;
        platform::log("audio: stalled at %.1f s, moving decoding to the app core", pos);
        audio::setSecondCoreAllowed(false);
        const core::Track &t = library.tracks()[nowTrack];
        std::string error;
        player.stop();
        if (player.play(t.path, t.format, error)) {
            if (pos > 1.0) player.seek(pos);
        } else {
            platform::log("audio: restart failed: %s", error.c_str());
        }
    }

    // ---- playback ----
    core::Item itemFor(int trackIndex) const
    {
        const core::Track &t = library.tracks()[trackIndex];
        core::Item item;
        item.libraryIndex = trackIndex;
        item.id = t.path;
        item.title = t.title;
        item.artist = t.artist;
        item.album = t.album;
        item.durationSeconds = t.durationSeconds;
        return item;
    }

    void applyQueueModes()
    {
        queue.setRepeat(settings.repeat == 1 ? core::Repeat::All : settings.repeat == 2 ? core::Repeat::One : core::Repeat::Off);
        queue.setShuffle(settings.shuffle, uint32_t(svcGetSystemTick() & 0xFFFFFFFF));
    }

    void playList(const std::vector<int> &trackIndices, int start, bool shuffleAll = false)
    {
        if (trackIndices.empty()) return;
        queue.clear();
        for (int i : trackIndices) queue.add(itemFor(i));
        if (shuffleAll) {
            settings.shuffle = true;
            settings.save();
            queue.jumpTo(size_t(start < 0 ? 0 : start));
            applyQueueModes();
            // Start on a random track rather than the first.
            queue.setShuffle(true, uint32_t(svcGetSystemTick() & 0xFFFFFFFF));
            queue.next(true);
        } else {
            queue.jumpTo(size_t(start < 0 ? 0 : start));
            applyQueueModes();
        }
        startCurrent();
        go(View::NowPlaying);
    }

    void startCurrent()
    {
        const core::Item *item = queue.current();
        if (item == nullptr) {
            player.stop();
            nowTrack = -1;
            return;
        }
        if (!audioReady) {
            nowTrack = item->libraryIndex;
            toast(audioError);
            return;
        }
        const core::Track &t = library.tracks()[item->libraryIndex];
        std::string error;
        if (!player.play(t.path, t.format, error)) {
            platform::log("play failed: %s (%s)", t.path.c_str(), error.c_str());
            toast(error);
            nowTrack = item->libraryIndex;
            return;
        }
        nowTrack = item->libraryIndex;
        focusNowQueue = -1;
        platform::log("playing: %s", t.path.c_str());
    }

    void skipNext(bool user)
    {
        if (queue.next(user)) startCurrent();
        else player.stop();
    }

    void skipPrevious()
    {
        if (player.positionSeconds() > 3.0) {
            player.seek(0.0);
            return;
        }
        if (queue.previous()) startCurrent();
        else player.seek(0.0);
    }

    void togglePlay()
    {
        if (player.loaded()) {
            player.toggle();
        } else if (queue.current() != nullptr) {
            startCurrent();
        } else if (!library.tracks().empty()) {
            std::vector<int> all;
            for (int i = 0; i < int(library.tracks().size()); ++i) all.push_back(i);
            playList(all, 0);
        }
    }

    const std::vector<int> &allTracks()
    {
        if (allTracksDirty) {
            allTracksCache.clear();
            for (const core::Album &a : library.albums()) {
                for (int t : a.tracks) allTracksCache.push_back(t);
            }
            allTracksDirty = false;
        }
        return allTracksCache;
    }

    void addToUpNext(const std::vector<int> &trackIndices)
    {
        for (int i : trackIndices) queue.add(itemFor(i));
        toast(plural(int(trackIndices.size()), "song added to up next", "songs added to up next"));
        if (!player.loaded() && queue.current() != nullptr) startCurrent();
    }

    // ---- menus ----
    void openTrackMenu(int trackIndex)
    {
        menu.clear();
        const int album = albumIndexOfTrack(trackIndex);
        menu.push_back({"Play now", Icon::Play, true, [this, trackIndex] { playList({trackIndex}, 0); }});
        menu.push_back({"Add to up next", Icon::Queue, true, [this, trackIndex] { addToUpNext({trackIndex}); }});
        if (album >= 0) {
            menu.push_back({"Play the album from here", Icon::Album, true, [this, album, trackIndex] {
                const core::Album &a = library.albums()[album];
                int start = 0;
                for (int i = 0; i < int(a.tracks.size()); ++i) if (a.tracks[i] == trackIndex) start = i;
                playList(a.tracks, start);
            }});
            menu.push_back({"Go to album", Icon::ChevronRight, true, [this, album] { openAlbum(album); }});
        }
        menuFocus = 0;
        menuOpen = true;
    }

    void openAlbumMenu(int albumIndex)
    {
        menu.clear();
        const core::Album &a = library.albums()[albumIndex];
        const std::vector<int> tracks = a.tracks;
        const std::string artist = a.artist;
        menu.push_back({"Play album", Icon::Play, true, [this, tracks] { playList(tracks, 0); }});
        menu.push_back({"Shuffle album", Icon::Shuffle, true, [this, tracks] { playList(tracks, 0, true); }});
        menu.push_back({"Add to up next", Icon::Queue, true, [this, tracks] { addToUpNext(tracks); }});
        menu.push_back({"Go to artist", Icon::Person, true, [this, artist] { openArtist(artist); }});
        menuFocus = 0;
        menuOpen = true;
    }

    void openAlbum(int albumIndex)
    {
        albumView = albumIndex;
        focusAlbum = 0;
        listScroll = 0; listPx = 0.0f;
        go(View::Album);
    }

    void openArtist(const std::string &artist)
    {
        artistView = artist;
        artistAlbums = library.albumsByArtist(artist);
        focusArtist = 0;
        listScroll = 0; listPx = 0.0f;
        go(View::Artist);
    }

    // ---- which album the top screen shows ----
    int showcaseAlbum()
    {
        switch (view) {
        case View::Section:
            switch (section) {
            case Section::Home: return focusHome < int(library.albums().size()) ? focusHome : -1;
            case Section::Songs: {
                const std::vector<int> &all = allTracks();
                return focusSongs < int(all.size()) ? albumIndexOfTrack(all[focusSongs]) : -1;
            }
            case Section::Artists: {
                const std::vector<std::string> artists = library.artists();
                if (focusArtists >= int(artists.size())) return -1;
                const std::vector<int> albums = library.albumsByArtist(artists[focusArtists]);
                return albums.empty() ? -1 : albums[0];
            }
            case Section::UpNext: {
                const std::vector<const core::Item *> upcoming = queue.upcoming();
                return focusUpNext < int(upcoming.size()) ? albumIndexOfTrack(upcoming[focusUpNext]->libraryIndex) : albumIndexOfTrack(nowTrack);
            }
            }
            return -1;
        case View::Album: return albumView;
        case View::Artist: return focusArtist < int(artistAlbums.size()) ? artistAlbums[focusArtist] : -1;
        case View::NowPlaying:
        case View::Visualizer: return albumIndexOfTrack(nowTrack);
        case View::Search: return focusSearch < int(searchResults.size()) ? albumIndexOfTrack(searchResults[focusSearch]) : -1;
        default: return -1;
        }
    }

    // ---- rows for list views ----
    std::vector<Row> rowsFor(Section sec)
    {
        std::vector<Row> rows;
        if (sec == Section::Songs) {
            for (int t : allTracks()) {
                const core::Track &track = library.tracks()[t];
                Row r;
                r.title = track.title;
                r.sub = track.artist;
                r.right = track.durationSeconds > 0 ? core::formatClock(track.durationSeconds) : "";
                r.album = albumIndexOfTrack(t);
                r.track = t;
                r.playing = t == nowTrack;
                rows.push_back(r);
            }
        } else if (sec == Section::Artists) {
            for (const std::string &artist : library.artists()) {
                const std::vector<int> albums = library.albumsByArtist(artist);
                int songs = 0;
                for (int a : albums) songs += int(library.albums()[a].tracks.size());
                Row r;
                r.title = artist;
                r.sub = plural(int(albums.size()), "album", "albums") + " · " + plural(songs, "song", "songs");
                r.album = albums.empty() ? -1 : albums[0];
                rows.push_back(r);
            }
        } else if (sec == Section::UpNext) {
            int n = 1;
            for (const core::Item *item : queue.upcoming()) {
                Row r;
                r.title = item->title;
                r.sub = item->artist;
                r.right = item->durationSeconds > 0 ? core::formatClock(item->durationSeconds) : "";
                r.album = albumIndexOfTrack(item->libraryIndex);
                r.track = item->libraryIndex;
                r.playing = n == 1 && item->libraryIndex == nowTrack;
                rows.push_back(r);
                ++n;
            }
        }
        return rows;
    }

    std::vector<Row> albumRows(int albumIndex)
    {
        std::vector<Row> rows;
        if (albumIndex < 0) return rows;
        const core::Album &a = library.albums()[albumIndex];
        for (int t : a.tracks) {
            const core::Track &track = library.tracks()[t];
            Row r;
            r.title = track.title;
            r.right = track.durationSeconds > 0 ? core::formatClock(track.durationSeconds) : "";
            r.sub = track.trackNumber > 0 ? std::to_string(track.trackNumber) : "";
            r.playing = t == nowTrack;
            rows.push_back(r);
        }
        return rows;
    }

    std::vector<Row> artistRows()
    {
        std::vector<Row> rows;
        for (int a : artistAlbums) {
            const core::Album &album = library.albums()[a];
            Row r;
            r.title = album.title;
            r.sub = (album.year.empty() ? "" : album.year + " · ") + plural(int(album.tracks.size()), "song", "songs");
            r.right = minutesText(album.durationSeconds);
            r.album = a;
            rows.push_back(r);
        }
        return rows;
    }

    std::vector<Row> searchRows()
    {
        std::vector<Row> rows;
        for (int t : searchResults) {
            const core::Track &track = library.tracks()[t];
            Row r;
            r.title = track.title;
            r.sub = track.artist + " · " + track.album;
            r.right = track.durationSeconds > 0 ? core::formatClock(track.durationSeconds) : "";
            r.album = albumIndexOfTrack(t);
            r.track = t;
            r.playing = t == nowTrack;
            rows.push_back(r);
        }
        return rows;
    }

    // ---- settings rows ----
    struct SettingRow {
        std::string label;
        std::string value;
        bool header;
        bool enabled;
        int toggle; // -1 none, 0 off, 1 on
        std::string help;
    };

    std::vector<SettingRow> settingRows()
    {
        std::vector<SettingRow> rows;
        const int songs = int(library.tracks().size());
        rows.push_back({"Playback", "", true, false, -1, ""});
        rows.push_back({"Keep playing with the lid closed", "", false, true, settings.keepPlayingLidClosed ? 1 : 0, "With this on, closing the lid turns the screens off and keeps the music going, like the console's own sound app. Open the lid and the screens come back."});
        rows.push_back({"Look", "", true, false, -1, ""});
        rows.push_back({"Dark mode", "", false, true, settings.darkMode ? 1 : 0, "Dark screens with light text. Off gives the white HOME Menu look. Either way the selection blue stays the same."});
        rows.push_back({"Full-screen album art", "", false, true, settings.artOnTopScreen ? 1 : 0, "On, the focused album's art fills the top screen behind the text. Off, it sits in a small sleeve on the left, which uses less battery on an Old 3DS."});
        rows.push_back({"Library", "", true, false, -1, ""});
        rows.push_back({"Music folder", settings.musicFolder, false, true, -1, "The folder 3dsFM scans for music. Albums are folders of songs; tags are read from MP3, Ogg, Opus and WAV files when they have them."});
        rows.push_back({"Rescan the Music folder", "", false, true, -1, "New files are picked up every time 3dsFM starts; this reads the tags of every file again, for when you have changed them."});
        rows.push_back({"Storage", plural(songs, "song", "songs") + " · " + gigabytes(platform::sdFreeBytes()) + " free", false, false, -1, "What is on the SD card: the songs in the Music folder and the space left for more."});
        return rows;
    }

    void activateSetting(int index)
    {
        std::vector<SettingRow> rows = settingRows();
        if (index < 0 || index >= int(rows.size())) return;
        const std::string &label = rows[index].label;
        if (label == "Keep playing with the lid closed") {
            settings.keepPlayingLidClosed = !settings.keepPlayingLidClosed;
            settings.save();
        } else if (label == "Dark mode") {
            settings.darkMode = !settings.darkMode;
            settings.save();
            ui::setTheme(settings.darkMode);
        } else if (label == "Full-screen album art") {
            settings.artOnTopScreen = !settings.artOnTopScreen;
            settings.save();
        } else if (label == "Music folder") {
            std::string folder;
            if (platform::keyboard("Folder on the SD card", settings.musicFolder, folder) && !folder.empty()) {
                if (folder.rfind("sdmc:/", 0) != 0) folder = "sdmc:/" + folder;
                while (folder.size() > 6 && folder.back() == '/') folder.pop_back();
                settings.musicFolder = folder;
                settings.save();
                player.stop();
                queue.clear();
                nowTrack = -1;
                startScan(true);
            }
        } else if (label == "Rescan the Music folder") {
            player.stop();
            queue.clear();
            nowTrack = -1;
            startScan(true);
            toast("Reading every file in " + settings.musicFolder);
        }
    }

    // ---- input ----
    void update()
    {
        ++frame;
        artLoadedThisFrame = false;
        resumeAfterHandover();
        watchForStall();
        stepScan();
        pollArt();
        if (frame % 60 == 1) refreshStatus();
        if (player.finished()) skipNext(false);
        if (toastFrames > 0) --toastFrames;

        hidScanInput();
        const u32 injected = smokeKey();
        const u32 k = keysDownRepeat() | injected;
        const u32 kd = hidKeysDown() | injected;
        touchPosition touch{};
        bool tapped = false;
        updateDrag(tapped, touch);
        // A key press cancels a flick.
        if ((k & ~u32(KEY_TOUCH)) != 0 && (drag.coasting || drag.moved) && !drag.held) {
            drag.coasting = false;
            drag.moved = false;
        }

        if (dialogOpen) {
            if (kd & KEY_A) closeDialog(true);
            else if (kd & KEY_B) closeDialog(false);
            else if (tapped) {
                if (ui::hit(170.0f, DialogButtonY, 70.0f, DialogButtonH, touch.px, touch.py)) closeDialog(false);
                else if (ui::hit(246.0f, DialogButtonY, 64.0f, DialogButtonH, touch.px, touch.py)) closeDialog(true);
            }
            return;
        }
        if (menuOpen) {
            updateMenu(k, kd, tapped, touch);
            return;
        }

        // Global keys.
        if (kd & KEY_START) {
            togglePlay();
        }
        if ((kd & KEY_L) && queue.current() != nullptr) skipPrevious();
        if ((kd & KEY_R) && queue.current() != nullptr) skipNext(true);
        if (kd & KEY_SELECT) {
            if (view == View::Visualizer) back(); else go(View::Visualizer);
        }

        // A tap takes the cursor out of the top row; keys up there stay there.
        if (chromeShown() && chromeFocus >= 0) {
            if (tapped) chromeFocus = -1;
            else if (updateChrome(k, kd)) return;
        }

        switch (view) {
        case View::Section: updateSection(k, kd, tapped, touch); break;
        case View::Album: updateAlbum(k, kd, tapped, touch); break;
        case View::Artist: updateArtist(k, kd, tapped, touch); break;
        case View::NowPlaying: updateNowPlaying(k, kd, tapped, touch); break;
        case View::Search: updateSearch(k, kd, tapped, touch); break;
        case View::Settings: updateSettings(k, kd, tapped, touch); break;
        case View::Visualizer: updateVisualizer(k, kd, tapped, touch); break;
        }
    }

    // The top row of the bottom screen, shared by section views and most others.
    bool chromeTap(const touchPosition &touch, bool withTabs)
    {
        if (withTabs) {
            for (int i = 0; i < 4; ++i) {
                if (ui::hit(TabX0 + i * TabStep - 13.0f, TabY - 13.0f, 26.0f, 26.0f, touch.px, touch.py)) {
                    section = Section(i);
                    listScroll = 0; listPx = 0.0f;
                    if (view != View::Section) go(View::Section);
                    return true;
                }
            }
        }
        for (int i = 0; i < 3; ++i) {
            if (ui::hit(RoundX0 + i * RoundStep - 14.0f, RoundY - 14.0f, 28.0f, 28.0f, touch.px, touch.py)) {
                if (i == 0) { go(View::Search); }
                else if (i == 1) { go(View::NowPlaying); }
                else { go(View::Settings); }
                return true;
            }
        }
        return false;
    }

    void moveListFocus(int &focus, int count, int visible, u32 k)
    {
        if (count <= 0) { focus = 0; listScroll = 0; listPx = 0.0f; return; }
        if (focus >= count) focus = count - 1;
        const bool navigated = up(k) || down(k);
        if (!navigated) return;
        // After a drag the cursor may be off screen: bring the cursor to the
        // list rather than the list to the cursor.
        if (focus < listScroll || focus >= listScroll + visible) {
            focus = std::max(listScroll, std::min(listScroll + visible - 1, focus));
            focus = std::min(focus, count - 1);
        }
        if (up(k)) focus = (focus + count - 1) % count;
        if (down(k)) focus = (focus + 1) % count;
        if (focus < listScroll) { listScroll = focus; listPx = listScroll * RowStep; }
        if (focus >= listScroll + visible) { listScroll = focus - visible + 1; listPx = listScroll * RowStep; }
    }

    int tappedRow(const touchPosition &touch, float y0, int visible, int count)
    {
        if (touch.py < y0 || touch.py >= y0 + visible * RowStep) return -1;
        const float px = scrollPx();
        const int index = int((touch.py - y0 + px) / RowStep);
        const float rowTop = y0 + index * RowStep - px;
        if (index < 0 || index >= count) return -1;
        if (!ui::hit(RowX, rowTop, RowW, RowH, touch.px, touch.py)) return -1;
        return index;
    }

    void updateSection(u32 k, u32 kd, bool tapped, const touchPosition &touch)
    {
        if (tapped && chromeTap(touch, true)) return;
        if (kd & KEY_B) {
            if (section != Section::Home) { section = Section::Home; listScroll = 0; listPx = 0.0f; }
            return;
        }
        switch (section) {
        case Section::Home: {
            const int count = int(library.albums().size());
            if (count == 0) break;
            if (focusHome >= count) focusHome = count - 1;
            if (up(k) && focusHome < GridCols) { enterChrome(); break; }
            const bool navigated = left(k) || right(k) || up(k) || down(k);
            if (navigated) {
                // Off-screen cursor after a drag: bring it to the visible rows first.
                const int focusRow = focusHome / GridCols;
                if (focusRow < scrollRows || focusRow >= scrollRows + GridRows) {
                    const int targetRow = std::max(scrollRows, std::min(scrollRows + GridRows - 1, focusRow));
                    focusHome = std::min(count - 1, targetRow * GridCols + focusHome % GridCols);
                }
                if (left(k) && focusHome > 0) --focusHome;
                if (right(k) && focusHome + 1 < count) ++focusHome;
                if (up(k)) focusHome = focusHome >= GridCols ? focusHome - GridCols : focusHome;
                if (down(k)) {
                    if (focusHome + GridCols < count) focusHome += GridCols;
                    else if (focusHome / GridCols < (count - 1) / GridCols) focusHome = count - 1;
                }
                const int row = focusHome / GridCols;
                if (row < scrollRows) { scrollRows = row; gridPx = scrollRows * TileStep; }
                if (row >= scrollRows + GridRows) { scrollRows = row - GridRows + 1; gridPx = scrollRows * TileStep; }
            }
            if (kd & KEY_A) openAlbum(focusHome);
            if (kd & KEY_Y) openAlbumMenu(focusHome);
            if (tapped) {
                if (ui::hit(StripX, StripY, StripW, StripH, touch.px, touch.py)) {
                    const float cy = StripY + StripH / 2.0f;
                    if (ui::hit(StripX + StripW - 24.0f - 14.0f, cy - 14.0f, 28.0f, 28.0f, touch.px, touch.py)) openAlbumMenu(focusHome);
                    else if (ui::hit(StripX + StripW - 54.0f - 14.0f, cy - 14.0f, 28.0f, 28.0f, touch.px, touch.py)) playList(library.albums()[focusHome].tracks, 0);
                    else openAlbum(focusHome);
                    return;
                }
                const float px = scrollPx();
                if (touch.py >= GridY && touch.py < GridY + GridRows * TileStep) {
                    const int r = int((touch.py - GridY + px) / TileStep);
                    for (int c = 0; c < GridCols; ++c) {
                        const int index = r * GridCols + c;
                        if (index >= count) continue;
                        if (ui::hit(GridX + c * TileStep, GridY + r * TileStep - px, Tile, Tile, touch.px, touch.py)) {
                            if (index == focusHome) openAlbum(index); else focusHome = index;
                            return;
                        }
                    }
                }
            }
            break;
        }
        case Section::Songs: {
            const std::vector<int> &all = allTracks();
            if (up(k) && focusSongs == 0) { enterChrome(); break; }
            moveListFocus(focusSongs, int(all.size()), SectionRows, k);
            if (all.empty()) break;
            if (kd & KEY_A) playList(all, focusSongs);
            if (kd & KEY_Y) openTrackMenu(all[focusSongs]);
            if (tapped) {
                const int row = tappedRow(touch, GridY, SectionRows, int(all.size()));
                if (row >= 0) { if (row == focusSongs) playList(all, row); else focusSongs = row; }
            }
            break;
        }
        case Section::Artists: {
            const std::vector<std::string> artists = library.artists();
            if (up(k) && focusArtists == 0) { enterChrome(); break; }
            moveListFocus(focusArtists, int(artists.size()), SectionRows, k);
            if (artists.empty()) break;
            if (kd & KEY_A) openArtist(artists[focusArtists]);
            if (tapped) {
                const int row = tappedRow(touch, GridY, SectionRows, int(artists.size()));
                if (row >= 0) { if (row == focusArtists) openArtist(artists[row]); else focusArtists = row; }
            }
            break;
        }
        case Section::UpNext: {
            const std::vector<const core::Item *> upcoming = queue.upcoming();
            if (up(k) && focusUpNext == 0) { enterChrome(); break; }
            moveListFocus(focusUpNext, int(upcoming.size()), SectionRows, k);
            if (upcoming.empty()) break;
            auto queueIndexOf = [&](int upcomingIndex) {
                const core::Item *target = upcoming[upcomingIndex];
                for (std::size_t i = 0; i < queue.size(); ++i) {
                    if (queue.upcoming()[0] == target && upcomingIndex == 0) return int(queue.position());
                }
                // upcoming() lists tracks in play order; map back by pointer.
                const std::vector<const core::Item *> all = queue.upcoming();
                (void)all;
                return -1;
            };
            (void)queueIndexOf;
            if (kd & KEY_A) {
                // Jump: advance until the focused entry is current.
                for (int i = 0; i < focusUpNext; ++i) queue.next(true);
                startCurrent();
                focusUpNext = 0;
            }
            if (kd & KEY_X) {
                if (focusUpNext == 0) {
                    toast("Skip the playing song with R");
                } else {
                    // Find the queue index of the focused upcoming item.
                    const core::Item *target = upcoming[focusUpNext];
                    for (std::size_t i = 0; i < queue.size(); ++i) {
                        // Items are stored contiguously; compare addresses.
                        if (&queue.upcoming()[0][0] == target) break;
                    }
                    removeUpcoming(focusUpNext);
                }
            }
            if (kd & KEY_Y && upcoming[focusUpNext]->libraryIndex >= 0) openTrackMenu(upcoming[focusUpNext]->libraryIndex);
            if (tapped) {
                const int row = tappedRow(touch, GridY, SectionRows, int(upcoming.size()));
                if (row >= 0) {
                    if (row == focusUpNext) {
                        for (int i = 0; i < row; ++i) queue.next(true);
                        startCurrent();
                        focusUpNext = 0;
                    } else {
                        focusUpNext = row;
                    }
                }
            }
            break;
        }
        }
    }

    void removeUpcoming(int upcomingIndex)
    {
        // Walk the queue to find which stored index plays at this position.
        const std::vector<const core::Item *> upcoming = queue.upcoming();
        if (upcomingIndex < 0 || upcomingIndex >= int(upcoming.size())) return;
        const core::Item *target = upcoming[upcomingIndex];
        for (std::size_t i = 0; i < queue.size(); ++i) {
            core::PlayQueue probe = queue;
            probe.jumpTo(i);
            if (probe.current() == &queue.upcoming()[0][0]) break;
        }
        // Items live in the queue's own vector, so the pointer difference from
        // the first stored item gives the index.
        const core::Item *first = nullptr;
        {
            core::PlayQueue probe = queue;
            probe.jumpTo(0);
            first = probe.current();
        }
        (void)first;
        // Simplest reliable path: rebuild without the target.
        std::vector<core::Item> kept;
        std::size_t newCursor = 0;
        const core::Item *current = queue.current();
        for (const core::Item *item : upcoming) {
            if (item == target) continue;
            if (item == current) newCursor = kept.size();
            kept.push_back(*item);
        }
        queue.clear();
        for (const core::Item &item : kept) queue.add(item);
        queue.jumpTo(newCursor);
        queue.setRepeat(settings.repeat == 1 ? core::Repeat::All : settings.repeat == 2 ? core::Repeat::One : core::Repeat::Off);
        toast("Removed from up next");
        if (focusUpNext >= int(queue.upcoming().size())) focusUpNext = std::max(0, int(queue.upcoming().size()) - 1);
    }

    void updateAlbum(u32 k, u32 kd, bool tapped, const touchPosition &touch)
    {
        if (albumView < 0 || albumView >= int(library.albums().size())) { view = View::Section; return; }
        const core::Album &a = library.albums()[albumView];
        if (tapped && chromeTap(touch, true)) return;
        if (kd & KEY_B) { back(); return; }
        if (up(k) && focusAlbum == 0) { enterChrome(); return; }
        moveListFocus(focusAlbum, int(a.tracks.size()), HeaderRows, k);
        if (kd & KEY_A && !a.tracks.empty()) playList(a.tracks, focusAlbum);
        if (kd & KEY_Y && !a.tracks.empty()) openTrackMenu(a.tracks[focusAlbum]);
        if (tapped) {
            if (ui::hit(252.0f, HeaderY + 7.0f, 26.0f, 26.0f, touch.px, touch.py)) playList(a.tracks, 0, true);
            else if (ui::hit(282.0f, HeaderY + 5.0f, 30.0f, 30.0f, touch.px, touch.py)) playList(a.tracks, 0);
            else {
                const int row = tappedRow(touch, HeaderRowsY, HeaderRows, int(a.tracks.size()));
                if (row >= 0) { if (row == focusAlbum) playList(a.tracks, row); else focusAlbum = row; }
            }
        }
    }

    void updateArtist(u32 k, u32 kd, bool tapped, const touchPosition &touch)
    {
        if (tapped && chromeTap(touch, true)) return;
        if (kd & KEY_B) { back(); return; }
        if (up(k) && focusArtist == 0) { enterChrome(); return; }
        moveListFocus(focusArtist, int(artistAlbums.size()), HeaderRows, k);
        if (artistAlbums.empty()) return;
        if (kd & KEY_A) openAlbum(artistAlbums[focusArtist]);
        if (kd & KEY_Y) openAlbumMenu(artistAlbums[focusArtist]);
        if (tapped) {
            const int row = tappedRow(touch, HeaderRowsY, HeaderRows, int(artistAlbums.size()));
            if (row >= 0) { if (row == focusArtist) openAlbum(artistAlbums[row]); else focusArtist = row; }
        }
    }

    void updateNowPlaying(u32 k, u32 kd, bool tapped, const touchPosition &touch)
    {
        if (tapped && chromeTap(touch, true)) return;
        if (kd & KEY_B) { back(); return; }
        // Transport focus: 0 shuffle, 1 prev, 2 play, 3 next, 4 repeat.
        // Down moves the cursor into the Up next rows, Up brings it back.
        const std::vector<const core::Item *> queued = queue.upcoming();
        const int queueRows = std::min(NowQueueRows, int(queued.size()) - 1);
        if (focusNowQueue < 0) {
            if (up(k)) { enterChrome(); return; }
            if (left(k)) focusNow = (focusNow + 4) % 5;
            if (right(k)) focusNow = (focusNow + 1) % 5;
            if (down(k) && queueRows > 0) focusNowQueue = 1;
            if (kd & KEY_A) transportAction(focusNow);
            if (kd & KEY_Y && nowTrack >= 0) openTrackMenu(nowTrack);
        } else {
            if (focusNowQueue > queueRows) focusNowQueue = queueRows;
            if (focusNowQueue <= 0) {
                focusNowQueue = -1;
            } else {
                if (up(k)) focusNowQueue = focusNowQueue > 1 ? focusNowQueue - 1 : -1;
                if (down(k) && focusNowQueue < queueRows) ++focusNowQueue;
                if (focusNowQueue > 0 && (kd & KEY_A)) {
                    for (int j = 0; j < focusNowQueue; ++j) queue.next(true);
                    startCurrent();
                    focusNowQueue = -1;
                    return;
                }
                if (focusNowQueue > 0 && (kd & KEY_Y)) openTrackMenu(queued[size_t(focusNowQueue)]->libraryIndex);
            }
        }
        if (tapped) {
            for (int i = 0; i < 5; ++i) {
                const float r = TransportR[i];
                if (ui::hit(TransportX[i] - r - 3, NowButtonsY - r - 3, r * 2 + 6, r * 2 + 6, touch.px, touch.py)) {
                    focusNow = i;
                    transportAction(i);
                    return;
                }
            }
            if (ui::hit(40.0f, NowSeekY - 8.0f, 240.0f, 18.0f, touch.px, touch.py) && nowTrack >= 0) {
                const int duration = library.tracks()[nowTrack].durationSeconds;
                if (duration > 0) player.seek(std::max(0.0, std::min(1.0, double(touch.px - 40.0f) / 240.0)) * duration);
                return;
            }
            const std::vector<const core::Item *> upcoming = queue.upcoming();
            for (int i = 1; i <= NowQueueRows && i < int(upcoming.size()); ++i) {
                if (ui::hit(RowX, NowQueueY + (i - 1) * RowStep, RowW, RowH, touch.px, touch.py)) {
                    if (focusNowQueue == i) {
                        for (int j = 0; j < i; ++j) queue.next(true);
                        startCurrent();
                        focusNowQueue = -1;
                    } else {
                        focusNowQueue = i;
                    }
                    return;
                }
            }
            focusNowQueue = -1;
        }
    }

    void transportAction(int which)
    {
        switch (which) {
        case 0:
            settings.shuffle = !settings.shuffle;
            settings.save();
            queue.setShuffle(settings.shuffle, uint32_t(svcGetSystemTick() & 0xFFFFFFFF));
            toast(settings.shuffle ? "Shuffle on" : "Shuffle off");
            break;
        case 1: skipPrevious(); break;
        case 2: togglePlay(); break;
        case 3: skipNext(true); break;
        case 4:
            settings.repeat = (settings.repeat + 1) % 3;
            settings.save();
            applyQueueModes();
            queue.setShuffle(settings.shuffle, uint32_t(svcGetSystemTick() & 0xFFFFFFFF));
            toast(settings.repeat == 0 ? "Repeat off" : settings.repeat == 1 ? "Repeat all" : "Repeat one");
            break;
        }
    }

    void runSearch()
    {
        std::string query;
        if (platform::keyboard("Song, artist or album", searchQuery, query)) {
            searchQuery = query;
            searchResults = library.search(query);
            focusSearch = 0;
            listScroll = 0; listPx = 0.0f;
        }
    }

    void updateSearch(u32 k, u32 kd, bool tapped, const touchPosition &touch)
    {
        if (kd & KEY_B) { back(); return; }
        if (tapped && ui::hit(10.0f, 10.0f, 300.0f, SearchBarH, touch.px, touch.py)) { runSearch(); return; }
        if (searchQuery.empty() && (kd & KEY_A)) { runSearch(); return; }
        moveListFocus(focusSearch, int(searchResults.size()), SearchRows, k);
        if (searchResults.empty()) {
            if (kd & KEY_A) runSearch();
            return;
        }
        if (kd & KEY_A) playList(searchResults, focusSearch);
        if (kd & KEY_Y) openTrackMenu(searchResults[focusSearch]);
        if (tapped) {
            const int row = tappedRow(touch, SearchRowsY, SearchRows, int(searchResults.size()));
            if (row >= 0) { if (row == focusSearch) playList(searchResults, row); else focusSearch = row; }
        }
    }

    void updateSettings(u32 k, u32 kd, bool tapped, const touchPosition &touch)
    {
        if (kd & KEY_B) { back(); return; }
        const std::vector<SettingRow> rows = settingRows();
        const int count = int(rows.size());
        auto step = [&](int dir) {
            for (int i = 0; i < count; ++i) {
                focusSettings = (focusSettings + dir + count) % count;
                if (!rows[focusSettings].header) break;
            }
        };
        if (up(k)) { step(-1); scrollSettingsToFocus(rows); }
        if (down(k)) { step(1); scrollSettingsToFocus(rows); }
        if (kd & KEY_A) activateSetting(focusSettings);
        if (tapped) {
            const std::vector<float> tops = settingTops(rows);
            for (int i = 0; i < count; ++i) {
                if (!rows[i].header && ui::hit(10.0f, tops[i], 300.0f, SettingRowH, touch.px, touch.py)) {
                    if (focusSettings == i) activateSetting(i); else focusSettings = i;
                    return;
                }
            }
        }
    }

    // The settings list is taller than the screen, so it scrolls to keep
    // the focused row in view. Both drawing and touch use these tops.
    float settingsScrollPx = 0.0f;
    std::vector<float> settingTops(const std::vector<SettingRow> &rows) const
    {
        std::vector<float> tops;
        float y = 12.0f - settingsScrollPx;
        for (const SettingRow &r : rows) {
            tops.push_back(y);
            y += (r.header ? SettingHeaderH : SettingRowH) + 1.0f;
        }
        return tops;
    }
    float settingsHeight(const std::vector<SettingRow> &rows) const
    {
        float h = 4.0f;
        for (const SettingRow &r : rows) h += (r.header ? SettingHeaderH : SettingRowH) + 1.0f;
        return h;
    }
    void scrollSettingsToFocus(const std::vector<SettingRow> &rows)
    {
        if (focusSettings < 0 || focusSettings >= int(rows.size())) return;
        float top = 12.0f;
        for (int i = 0; i < focusSettings; ++i) top += (rows[i].header ? SettingHeaderH : SettingRowH) + 1.0f;
        // Keep the group header above the focused row in view too.
        float visibleTop = top;
        if (focusSettings > 0 && rows[focusSettings - 1].header) visibleTop -= SettingHeaderH + 1.0f;
        const float bottom = top + SettingRowH;
        const float maxScroll = std::max(0.0f, settingsHeight(rows) + 8.0f - BotH);
        if (bottom - settingsScrollPx > BotH - 6.0f) settingsScrollPx = bottom - (BotH - 6.0f);
        if (visibleTop - settingsScrollPx < 12.0f) settingsScrollPx = visibleTop - 12.0f;
        settingsScrollPx = std::max(0.0f, std::min(maxScroll, settingsScrollPx));
    }

    void updateVisualizer(u32 k, u32 kd, bool tapped, const touchPosition &touch)
    {
        if (kd & KEY_B) { back(); return; }
        if (kd & KEY_A) togglePlay();
        if (left(k)) { settings.visualizerStyle = (settings.visualizerStyle + 3) % 4; settings.save(); }
        if (right(k)) { settings.visualizerStyle = (settings.visualizerStyle + 1) % 4; settings.save(); }
        if (tapped) {
            for (int i = 0; i < 4; ++i) {
                if (ui::hit(34.0f + i * 63.0f, 74.0f, 61.0f, 24.0f, touch.px, touch.py)) { settings.visualizerStyle = i; settings.save(); return; }
            }
            if (ui::hit(142.0f, 150.0f, 36.0f, 36.0f, touch.px, touch.py)) togglePlay();
            else if (ui::hit(30.0f, 110.0f, 260.0f, 38.0f, touch.px, touch.py)) go(View::NowPlaying);
        }
        // Dim the bottom screen after a while without input.
        if (kd != 0 || tapped) visualizerDim = 0; else if (visualizerDim < 600) ++visualizerDim;
    }

    void updateMenu(u32 k, u32 kd, bool tapped, const touchPosition &touch)
    {
        const int count = int(menu.size());
        if (up(k)) menuFocus = (menuFocus + count - 1) % count;
        if (down(k)) menuFocus = (menuFocus + 1) % count;
        if (kd & KEY_B) { menuOpen = false; return; }
        if (kd & KEY_A) {
            menuOpen = false;
            MenuItem item = menu[menuFocus];
            item.action();
            return;
        }
        if (tapped) {
            for (int i = 0; i < count; ++i) {
                if (ui::hit(MenuX, MenuY + 4.0f + i * MenuRowStep, MenuW, MenuRowH, touch.px, touch.py)) {
                    menuOpen = false;
                    MenuItem item = menu[i];
                    item.action();
                    return;
                }
            }
            if (ui::hit(MenuX + MenuW + 6.0f, MenuY - 2.0f, 28.0f, 28.0f, touch.px, touch.py)) menuOpen = false;
            else if (!ui::hit(MenuX, MenuY, MenuW, 8.0f + count * MenuRowStep, touch.px, touch.py)) menuOpen = false;
        }
    }

    // ---- drawing: top screen ----
    // Top screen grid. A top band for corner metadata (centred on y 14), a
    // content band from 36 to 196 with the cover box on the left and the
    // text column beside it, and a bottom band with the playing thumbnail
    // and the legend.
    static constexpr float BandTopY = 14.0f;
    static constexpr float BoxX = 16.0f;
    static constexpr float BoxY = 40.0f;
    static constexpr float BoxSize = 150.0f;
    static constexpr float BoxCenterY = BoxY + BoxSize / 2.0f;
    static constexpr float TextXBox = BoxX + BoxSize + 18.0f;
    static constexpr float TextXFog = 165.0f;
    static constexpr float TextRight = TopW - 14.0f;
    static constexpr float CornerY = 206.0f;
    static constexpr float LegendY = 227.0f;
    bool fogMode = false; // set by drawShowcaseArt for the frame
    float textX() const { return fogMode ? TextXFog : TextXBox; }
    float textMaxW() const { return TextRight - textX(); }

    void drawShowcaseArt(int albumIndex, int trackIndex = -1)
    {
        const bool playlist = trackIndex < 0 && albumIndex >= 0 && albumIndex < int(library.albums().size()) && library.albums()[albumIndex].isPlaylist;
        const art::Image *image = playlist ? nullptr : trackIndex >= 0 ? trackArt(trackIndex) : albumArt(albumIndex);
        fogMode = settings.artOnTopScreen && image != nullptr;
        if (playlist) {
            ui::roundRect(BoxX, BoxY + 3.0f, BoxSize, BoxSize, 12.0f, ui::withAlpha(ui::ShadowBase, 0x16));
            drawMosaic(library.albums()[albumIndex], BoxX, BoxY, BoxSize, ui::TopBg);
            return;
        }
        if (fogMode) {
            art::draw(*image, -20.0f, -30.0f, 300.0f, 300.0f);
            // Fog the art into the white of the screen.
            ui::gradientH(110.0f, -30.0f, 170.0f, 300.0f, ui::withAlpha(ui::TopBg, 0), ui::TopBg);
            ui::rect(280.0f, 0.0f, 120.0f, TopH, ui::TopBg);
            ui::gradientV(-20.0f, 130.0f, 300.0f, 110.0f, ui::withAlpha(ui::TopBg, 0), ui::withAlpha(ui::TopBg, 235));
            // A stronger band at the top so the corner labels stay readable.
            ui::gradientV(-20.0f, 0.0f, 300.0f, 34.0f, ui::withAlpha(ui::TopBg, 225), ui::withAlpha(ui::TopBg, 120));
            ui::gradientV(-20.0f, 34.0f, 300.0f, 40.0f, ui::withAlpha(ui::TopBg, 120), ui::withAlpha(ui::TopBg, 0));
            return;
        }
        if (albumIndex < 0 && trackIndex < 0) return;
        // Framed cover, or a soft placeholder when there is no picture.
        ui::roundRect(BoxX, BoxY + 3.0f, BoxSize, BoxSize, 12.0f, ui::withAlpha(ui::ShadowBase, 0x16));
        if (image != nullptr) {
            art::draw(*image, BoxX, BoxY, BoxSize, BoxSize);
            ui::roundCorners(BoxX, BoxY, BoxSize, BoxSize, 12.0f, ui::TopBg, ui::TopBg);
        } else {
            ui::roundRect(BoxX, BoxY, BoxSize, BoxSize, 12.0f, ui::BottomBg);
            ui::icon(Icon::Note, BoxX + BoxSize / 2.0f, BoxCenterY, 50.0f, ui::Line, ui::BottomBg);
        }
    }

    void drawTopCorners(bool withRecent)
    {
        // Top-left: recent covers and a count, or the scan state. Views that
        // write their own label there (Settings, Search, albums, Now Playing)
        // pass false and keep the corner; the scan state shows on Home.
        const float ty = BandTopY - 5.0f; // 10 px text centred on the band
        if (scanning && withRecent) {
            const std::string verb = library.tracks().empty() ? "Scanning · " : "Checking · ";
            ui::text(verb + std::to_string(scanScanned()) + " of " + std::to_string(scanCollected()), BoxX, ty, 10.0f, ui::Meta);
        } else if (withRecent) {
            const int n = int(library.albums().size());
            float x = BoxX;
            for (int i = 0; i < 3 && i < n; ++i) {
                if (library.albums()[i].isPlaylist) {
                    drawMosaic(library.albums()[i], x, BandTopY - 7.0f, 14.0f, ui::TopBg);
                } else {
                    const art::Image *image = albumArt(i);
                    if (image != nullptr) {
                        art::draw(*image, x, BandTopY - 7.0f, 14.0f, 14.0f);
                        ui::roundCorners(x, BandTopY - 7.0f, 14.0f, 14.0f, 3.0f, ui::TopBg, ui::TopBg);
                    } else {
                        ui::roundRect(x, BandTopY - 7.0f, 14.0f, 14.0f, 3.0f, ui::Line);
                        ui::icon(Icon::Note, x + 7.0f, BandTopY, 8.0f, ui::Meta, ui::Line);
                    }
                }
                x += 18.0f;
            }
            if (n > 0) ui::text(plural(n, "album", "albums") + " · " + plural(int(library.tracks().size()), "song", "songs"), x + 2.0f, ty, 10.0f, ui::Meta);
        }
        float x = TextRight - ui::textWidth(status.clock, 10.0f);
        ui::text(status.clock, x, ty, 10.0f, ui::Meta);
        x -= 16.0f;
        ui::icon(Icon::Battery, x, BandTopY, 13.0f, ui::Meta, ui::TopBg);
        ui::rect(x - 5.0f, BandTopY - 2.2f, 8.5f * status.battery / 5.0f, 4.4f, status.charging ? ui::Blue : ui::Meta);
        x -= 18.0f;
        for (int bar = 0; bar < 3; ++bar) {
            const float bh = 4.0f + bar * 3.0f;
            ui::roundRect(x - 6.0f + bar * 4.5f, BandTopY + 5.0f - bh, 3.0f, bh, 1.0f, bar < status.wifi ? ui::Meta : ui::Line);
        }
        x -= 18.0f;
        if (status.headphones) ui::icon(Icon::Headphones, x, BandTopY, 13.0f, ui::Meta, ui::TopBg);
    }

    void drawPlayingCorner()
    {
        if (nowTrack < 0 || view == View::NowPlaying) return;
        const core::Track &t = library.tracks()[nowTrack];
        const art::Image *image = trackArt(nowTrack);
        if (image != nullptr) { art::draw(*image, BoxX, CornerY, 26.0f, 26.0f); ui::roundCorners(BoxX, CornerY, 26.0f, 26.0f, 5.0f, ui::TopBg, ui::TopBg); }
        else { ui::roundRect(BoxX, CornerY, 26.0f, 26.0f, 5.0f, ui::BottomBg); ui::icon(Icon::Note, BoxX + 13.0f, CornerY + 13.0f, 12.0f, ui::Meta, ui::BottomBg); }
        ui::marquee("corner", t.title, BoxX + 32.0f, CornerY + 1.0f, 10.0f, ui::Ink, 150.0f, ui::Weight::Bold);
        const std::string state = player.playing() ? "Playing" : player.loaded() ? "Paused" : "Stopped";
        ui::text(state + " · " + core::formatClock(int(player.positionSeconds())), BoxX + 32.0f, CornerY + 14.0f, 10.0f, ui::Meta);
    }

    void drawLegend(std::initializer_list<ui::LegendItem> items)
    {
        std::vector<ui::LegendItem> v(items);
        ui::legend(v.data(), int(v.size()), TextRight, LegendY);
    }

    // Empty states sit in the middle of the screen rather than in the text column.
    void drawEmptyTop(const std::string &title, const std::string &sub, const std::string &meta = "")
    {
        const float block = 24.0f + 6.0f + 13.0f + (meta.empty() ? 0.0f : 16.0f);
        float y = BoxCenterY - block / 2.0f;
        ui::text(title, TopW / 2.0f, y, 24.0f, ui::Ink, ui::Align::Center, 0.0f, ui::Weight::Bold);
        y += 30.0f;
        ui::text(sub, TopW / 2.0f, y, 13.0f, ui::InkSoft, ui::Align::Center);
        if (!meta.empty()) ui::text(meta, TopW / 2.0f, y + 18.0f, 10.0f, ui::Meta, ui::Align::Center);
    }

    void drawEmptyCard(Icon icon, const std::string &title, const std::string &line1, const std::string &line2 = "", const std::string &tagText = "")
    {
        const float h = 96.0f + (line2.empty() ? 0.0f : 12.0f) + (tagText.empty() ? 0.0f : 20.0f);
        const float y = (BotH + 30.0f) / 2.0f - h / 2.0f; // centred below the tab row
        ui::roundRect(30.0f, y + 2.0f, 260.0f, h, 8.0f, ui::withAlpha(ui::ShadowBase, 0x14));
        ui::roundRect(30.0f, y, 260.0f, h, 8.0f, ui::Sheet);
        ui::icon(icon, BotW / 2.0f, y + 24.0f, 24.0f, ui::Line, ui::Sheet);
        ui::text(title, BotW / 2.0f, y + 44.0f, 13.0f, ui::Ink, ui::Align::Center, 0.0f, ui::Weight::Bold);
        ui::text(line1, BotW / 2.0f, y + 64.0f, 10.0f, ui::Meta, ui::Align::Center, 240.0f);
        float next = y + 76.0f;
        if (!line2.empty()) { ui::text(line2, BotW / 2.0f, next, 10.0f, ui::Meta, ui::Align::Center, 240.0f); next += 12.0f; }
        if (!tagText.empty()) {
            const float w = ui::textWidth(tagText, 8.0f, ui::Weight::Bold) + 12.0f;
            ui::tag(tagText, BotW / 2.0f - w / 2.0f, next + 4.0f, ui::Line, ui::Meta);
        }
    }

    // The title block is centred on the cover box; `extra` is the height of
    // anything the caller draws below the metadata (the progress line).
    // Returns the y the title was drawn at.
    float drawShowcaseText(const std::string &title, const std::string &sub, const std::string &meta, float extra = 0.0f)
    {
        const float x = textX();
        const float maxW = textMaxW();
        const float size = fittedShowcaseSize(title);
        const float block = size + 6.0f + 13.0f + 3.0f + 10.0f + (meta.empty() ? -13.0f : 0.0f) + extra;
        const float y = BoxCenterY - block / 2.0f;
        ui::marquee("showcase", title, x, y, size, ui::Ink, maxW, ui::Weight::Bold);
        ui::marquee("showcase-sub", sub, x, y + size + 6.0f, 13.0f, ui::InkSoft, maxW);
        ui::text(meta, x, y + size + 22.0f, 10.0f, ui::Meta, ui::Align::Left, maxW);
        return y;
    }

    void drawContextLegend()
    {
        switch (view) {
        case View::Section:
            if (section == Section::Home) drawLegend({{"A", "Open"}, {"Y", "Menu"}});
            else if (section == Section::UpNext) drawLegend({{"A", "Play now"}, {"X", "Remove"}, {"Y", "Menu"}});
            else drawLegend({{"A", "Play"}, {"B", "Home"}, {"Y", "Menu"}});
            break;
        case View::Album:
        case View::Artist:
        case View::Search:
            drawLegend({{"A", "Play"}, {"B", "Back"}, {"Y", "Menu"}});
            break;
        case View::NowPlaying:
            drawLegend({{"A", player.playing() ? "Pause" : "Play"}, {"L", "Prev"}, {"R", "Next"}, {"Y", "Menu"}});
            break;
        default:
            break;
        }
    }

    void drawTop()
    {
        ui::rect(0, 0, TopW, TopH, ui::TopBg);
        const int album = showcaseAlbum();
        // Whatever the bottom screen is doing, the top screen shows what is
        // playing. Only the information views keep their own top screen.
        if (nowTrack >= 0 && view != View::Settings && view != View::Visualizer) {
            drawTopNowPlaying(albumIndexOfTrack(nowTrack));
            drawContextLegend();
            drawToast();
            return;
        }
        switch (view) {
        case View::Section: drawTopSection(album); break;
        case View::Album:
        case View::Artist: {
            drawShowcaseArt(album);
            drawTopCorners(false);
            ui::icon(Icon::Back, BoxX + 3.0f, BandTopY, 9.0f, ui::Meta, ui::TopBg);
            ui::text(view == View::Album ? (artistView.empty() ? "Library" : artistView) : "Artists", BoxX + 11.0f, BandTopY - 5.0f, 10.0f, ui::Meta);
            if (album >= 0) {
                const core::Album &a = library.albums()[album];
                std::string meta = (a.year.empty() ? "" : a.year + " · ") + plural(int(a.tracks.size()), "song", "songs") + " · " + minutesText(a.durationSeconds);
                const float y = drawShowcaseText(a.title, a.artist, meta, 22.0f);
                ui::tag(plural(int(a.tracks.size()), "song on the SD card", "songs on the SD card"), textX(), y + fittedShowcaseSize(a.title) + 40.0f, ui::Tag, ui::TagText);
            }
            drawPlayingCorner();
            drawLegend({{"A", "Play"}, {"B", "Back"}, {"Y", "Menu"}});
            break;
        }
        case View::NowPlaying: drawTopNowPlaying(album); break;
        case View::Search:
            drawShowcaseArt(album, (!searchResults.empty() && focusSearch < int(searchResults.size())) ? searchResults[focusSearch] : -1);
            drawTopCorners(false);
            ui::text(searchQuery.empty() ? "Search" : "Search · \"" + searchQuery + "\" · " + plural(int(searchResults.size()), "result", "results"), BoxX, BandTopY - 5.0f, 10.0f, ui::Meta);
            if (!searchResults.empty() && focusSearch < int(searchResults.size())) {
                const core::Track &t = library.tracks()[searchResults[focusSearch]];
                drawShowcaseText(t.title, t.artist, t.album + (t.durationSeconds > 0 ? " · " + core::formatClock(t.durationSeconds) : ""));
            } else {
                drawEmptyTop(searchQuery.empty() ? "Find a song" : "Nothing found", searchQuery.empty() ? "Press A to open the keyboard" : "Try part of a title, artist or album");
            }
            drawPlayingCorner();
            drawLegend({{"A", "Play"}, {"B", "Back"}, {"Y", "Menu"}});
            break;
        case View::Settings: drawTopSettings(); break;
        case View::Visualizer: drawTopVisualizer(album); break;
        }
        drawToast();
    }

    void drawTopSection(int album)
    {
        int focusedTrack = -1;
        if (section == Section::Songs) {
            const std::vector<int> &all = allTracks();
            if (focusSongs < int(all.size())) focusedTrack = all[focusSongs];
        } else if (section == Section::UpNext) {
            const std::vector<const core::Item *> upcoming = queue.upcoming();
            if (focusUpNext < int(upcoming.size())) focusedTrack = upcoming[focusUpNext]->libraryIndex;
        }
        drawShowcaseArt(album, focusedTrack);
        drawTopCorners(true);
        if (album >= 0 && album < int(library.albums().size())) {
            const core::Album &a = library.albums()[album];
            if (section == Section::Songs || section == Section::UpNext || section == Section::Home) {
                const int track = focusedTrack;
                if (track >= 0) {
                    const core::Track &t = library.tracks()[track];
                    drawShowcaseText(t.title, t.artist, t.album + (t.durationSeconds > 0 ? " · " + core::formatClock(t.durationSeconds) : "") + " · " + core::formatName(t.format));
                } else {
                    drawShowcaseText(a.title, a.artist, (a.year.empty() ? "" : a.year + " · ") + plural(int(a.tracks.size()), "song", "songs") + " · " + minutesText(a.durationSeconds));
                }
            } else {
                const std::vector<std::string> artists = library.artists();
                const std::string artist = focusArtists < int(artists.size()) ? artists[focusArtists] : a.artist;
                const std::vector<int> albums = library.albumsByArtist(artist);
                int songs = 0;
                for (int i : albums) songs += int(library.albums()[i].tracks.size());
                drawShowcaseText(artist, "Artist", plural(int(albums.size()), "album", "albums") + " · " + plural(songs, "song", "songs"));
            }
        } else if (!scanning && library.tracks().empty()) {
            drawEmptyTop("Nothing here yet", "Copy music to " + settings.musicFolder, "MP3, Ogg Vorbis, Opus and WAV");
        } else if (section == Section::UpNext) {
            drawEmptyTop("Up next is empty", "Pick an album or a song to start");
        }
        drawPlayingCorner();
        if (section == Section::Home) drawLegend({{"A", "Open"}, {"X", "Save"}, {"Y", "Menu"}});
        else if (section == Section::UpNext) drawLegend({{"A", "Play now"}, {"X", "Remove"}, {"Y", "Menu"}});
        else drawLegend({{"A", "Play"}, {"B", "Home"}, {"Y", "Menu"}});
    }

    void drawTopNowPlaying(int album)
    {
        drawShowcaseArt(album, nowTrack);
        drawTopCorners(false);
        const float level = player.level();
        ui::equaliser(BoxX, BandTopY - 6.0f, level, ui::Blue, int(frame));
        std::string state = player.playing() ? "Now playing" : player.loaded() ? "Paused" : "Stopped";
        if (nowTrack >= 0) state += std::string(" · ") + core::formatName(library.tracks()[nowTrack].format);
        const std::vector<const core::Item *> queued = queue.upcoming();
        if (queued.size() > 1) state += " · " + plural(int(queued.size()) - 1, "song", "songs") + " queued";
        ui::text(state, BoxX + 14.0f, BandTopY - 5.0f, 10.0f, ui::Meta);
        if (nowTrack >= 0) {
            const core::Track &t = library.tracks()[nowTrack];
            const float y = drawShowcaseText(t.title, t.artist, t.album + (library.albumOf(nowTrack) && !library.albumOf(nowTrack)->year.empty() ? " · " + library.albumOf(nowTrack)->year : ""), 30.0f);
            const float barY = y + fittedShowcaseSize(t.title) + 46.0f;
            const float pos = float(player.positionSeconds());
            const float fraction = t.durationSeconds > 0 ? pos / t.durationSeconds : 0.0f;
            ui::progressBar(textX(), barY, textMaxW(), 3.0f, fraction, ui::Line);
            ui::text(core::formatClock(int(pos)), textX(), barY + 7.0f, 10.0f, ui::Meta);
            ui::text(t.durationSeconds > 0 ? core::formatClock(t.durationSeconds) : "", TextRight, barY + 7.0f, 10.0f, ui::Meta, ui::Align::Right);
        } else {
            drawEmptyTop("Nothing playing", "Pick an album or a song", "START plays everything");
        }
        // Bottom-left: up next.
        const std::vector<const core::Item *> upcoming = queue.upcoming();
        if (upcoming.size() > 1) {
            const core::Item *next = upcoming[1];
            const art::Image *image = trackArt(next->libraryIndex);
            if (image != nullptr) { art::draw(*image, BoxX, CornerY, 26.0f, 26.0f); ui::roundCorners(BoxX, CornerY, 26.0f, 26.0f, 5.0f, ui::TopBg, ui::TopBg); }
            else { ui::roundRect(BoxX, CornerY, 26.0f, 26.0f, 5.0f, ui::BottomBg); ui::icon(Icon::Note, BoxX + 13.0f, CornerY + 13.0f, 12.0f, ui::Meta, ui::BottomBg); }
            ui::marquee("corner", next->title, BoxX + 32.0f, CornerY + 1.0f, 10.0f, ui::Ink, 150.0f, ui::Weight::Bold);
            ui::text("Up next · " + next->artist, BoxX + 32.0f, CornerY + 14.0f, 10.0f, ui::Meta, ui::Align::Left, 150.0f);
        } else if (nowTrack >= 0) {
            ui::text("Last song in up next", BoxX, CornerY + 8.0f, 10.0f, ui::Meta);
        }
        if (view == View::NowPlaying) drawLegend({{"A", player.playing() ? "Pause" : "Play"}, {"L", "Prev"}, {"R", "Next"}, {"Y", "Menu"}});
    }

    void drawTopSettings()
    {
        drawTopCorners(false);
        ui::text("Settings · 3dsFM " APP_VERSION " · " + std::string(platform::isNew3ds() ? "New 3DS" : "3DS"), BoxX, BandTopY - 5.0f, 10.0f, ui::Meta);
        const std::vector<SettingRow> rows = settingRows();
        if (focusSettings >= 0 && focusSettings < int(rows.size())) {
            const SettingRow &r = rows[focusSettings];
            ui::roundRect(75.0f, 57.0f, 250.0f, 110.0f, 8.0f, ui::withAlpha(ui::ShadowBase, 0x14));
            ui::roundRect(75.0f, 55.0f, 250.0f, 110.0f, 8.0f, ui::Sheet);
            ui::text(r.label, 87.0f, 66.0f, 15.0f, ui::Ink, ui::Align::Left, 226.0f, ui::Weight::Bold);
            // Wrap the help text by words into 8 px lines.
            std::string line;
            float y = 86.0f;
            std::string word;
            std::vector<std::string> lines;
            for (size_t i = 0; i <= r.help.size(); ++i) {
                const char c = i < r.help.size() ? r.help[i] : ' ';
                if (c == ' ') {
                    const std::string candidate = line.empty() ? word : line + " " + word;
                    if (ui::textWidth(candidate, 10.0f) > 226.0f && !line.empty()) {
                        lines.push_back(line);
                        line = word;
                    } else {
                        line = candidate;
                    }
                    word.clear();
                } else {
                    word += c;
                }
            }
            if (!line.empty()) lines.push_back(line);
            for (const std::string &l : lines) {
                if (y > 150.0f) break;
                ui::text(l, 87.0f, y, 10.0f, ui::InkSoft);
                y += 11.0f;
            }
        }
        drawPlayingCorner();
        drawLegend({{"A", "Change"}, {"B", "Back"}});
    }

    void drawTopVisualizer(int album)
    {
        const art::Image *image = nowTrack >= 0 ? trackArt(nowTrack) : albumArt(album);
        const u32 colorA = image != nullptr ? image->colorA : ui::Ink;
        const u32 colorB = image != nullptr ? image->colorB : ui::BlueDeep;
        computeBands();
        const int style = settings.visualizerStyle;
        if (style == 3) {
            if (image != nullptr) {
                art::draw(*image, 110.0f, 30.0f, 180.0f, 180.0f);
            } else {
                ui::roundRect(110.0f, 30.0f, 180.0f, 180.0f, 12.0f, ui::BottomBg);
                ui::icon(Icon::Note, 200.0f, 120.0f, 60.0f, ui::Line, ui::BottomBg);
            }
        } else if (style == 0) {
            for (int i = 0; i < 20; ++i) {
                const float h = 4.0f + bands[i] * 150.0f;
                const float x = 20.0f + i * 18.0f;
                ui::roundRect(x, 210.0f - h, 14.0f, h, 3.0f, (i % 2 == 0) ? colorA : colorB);
                ui::roundRect(x, 210.0f - h - 7.0f, 14.0f, 3.0f, 1.5f, ui::withAlpha((i % 2 == 0) ? colorA : colorB, 0x60));
            }
        } else if (style == 1) {
            float samples[200];
            const int n = player.recentSamples(samples, 200);
            for (int i = 1; i < n; ++i) {
                const float x0 = 20.0f + (i - 1) * 360.0f / n;
                const float x1 = 20.0f + i * 360.0f / n;
                ui::line(x0, 125.0f - samples[i - 1] * 90.0f, x1, 125.0f - samples[i] * 90.0f, 2.0f, (i % 40 < 20) ? colorA : colorB);
            }
        } else {
            for (int i = 0; i < 20; ++i) {
                const float r = 2.0f + bands[i] * 16.0f;
                ui::circle(30.0f + i * 18.0f, 120.0f - bands[i] * 70.0f, r, (i % 2 == 0) ? colorA : colorB);
                ui::circle(30.0f + i * 18.0f, 120.0f + bands[i] * 70.0f, r * 0.6f, ui::withAlpha((i % 2 == 0) ? colorA : colorB, 0x80));
            }
        }
        // Corner: what is playing.
        if (nowTrack >= 0) {
            const core::Track &t = library.tracks()[nowTrack];
            if (image != nullptr) { art::draw(*image, BoxX, 8.0f, 24.0f, 24.0f); ui::roundCorners(BoxX, 8.0f, 24.0f, 24.0f, 5.0f, ui::TopBg, ui::TopBg); }
            const float tx = image != nullptr ? BoxX + 30.0f : BoxX;
            ui::marquee("viz", t.title, tx, 6.0f, 12.0f, ui::Ink, 200.0f, ui::Weight::Bold);
            ui::text(t.artist + " · " + core::formatClock(int(player.positionSeconds())) + (t.durationSeconds > 0 ? " / " + core::formatClock(t.durationSeconds) : ""), tx, 21.0f, 10.0f, ui::Meta, ui::Align::Left, 200.0f);
        }
        ui::text(status.clock, TextRight, BandTopY - 5.0f, 10.0f, ui::Meta, ui::Align::Right);
        static const char *names[4] = {"Bars", "Wave", "Dots", "Art only"};
        ui::text(std::string(names[style]) + " · colors from the album art", BoxX, LegendY - 5.0f, 10.0f, ui::Meta);
        drawLegend({{"A", player.playing() ? "Pause" : "Play"}, {"B", "Back"}, {"L", "Prev"}, {"R", "Next"}});
    }

    void computeBands()
    {
        float samples[512];
        const int n = player.recentSamples(samples, 512);
        if (n < 64 || !player.playing()) {
            for (float &b : bands) b *= 0.85f;
            return;
        }
        // Goertzel at 20 log-spaced frequencies, assuming 44.1 kHz; the
        // picture is decorative so the exact rate does not matter.
        const float rate = 44100.0f;
        for (int b = 0; b < 20; ++b) {
            const float freq = 60.0f * std::pow(200.0f, float(b) / 19.0f);
            const float w = 2.0f * 3.14159265f * freq / rate;
            const float coeff = 2.0f * std::cos(w);
            float s0 = 0.0f;
            float s1 = 0.0f;
            float s2 = 0.0f;
            for (int i = 0; i < n; ++i) {
                s0 = samples[i] + coeff * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            const float power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
            float value = std::sqrt(power > 0.0f ? power : 0.0f) / (n * 0.08f);
            value = std::log10(1.0f + value * 9.0f);
            if (value > 1.0f) value = 1.0f;
            bands[b] = bands[b] * 0.6f + value * 0.4f;
        }
    }

    void drawToast()
    {
        if (toastFrames <= 0) return;
        const float w = ui::textWidth(toastText, 10.0f) + 24.0f;
        const float x = (TopW - w) / 2.0f;
        const float alpha = toastFrames < 20 ? toastFrames / 20.0f : 1.0f;
        ui::roundRect(x, 183.0f, w, 20.0f, 10.0f, ui::withAlpha(ui::Ink, u8(0x18 * alpha)));
        ui::roundRect(x, 181.0f, w, 20.0f, 10.0f, ui::withAlpha(ui::Sheet, u8(255 * alpha)));
        ui::icon(Icon::Check, x + 12.0f, 191.0f, 9.0f, ui::withAlpha(ui::Blue, u8(255 * alpha)), ui::Sheet);
        ui::text(toastText, x + 22.0f, 187.0f, 10.0f, ui::withAlpha(ui::Ink, u8(255 * alpha)));
    }

    // ---- drawing: bottom screen ----
    void drawBottomChrome(bool withTabs)
    {
        if (withTabs) {
            static const Icon icons[4] = {Icon::Home, Icon::Note, Icon::Person, Icon::Queue};
            for (int i = 0; i < 4; ++i) {
                const bool active = view == View::Section && int(section) == i;
                const bool cursor = chromeFocus == i;
                if (cursor) ui::circle(TabX0 + i * TabStep, TabY, 15.0f, ui::BlueTint);
                ui::icon(icons[i], TabX0 + i * TabStep, TabY, TabIcon, active || cursor ? ui::Blue : ui::Meta, cursor ? ui::BlueTint : ui::BottomBg);
            }
        }
        // One cursor on the whole screen: the ring is the cursor, and the
        // button of the current view shows it by a blue icon instead.
        ui::roundButton(RoundX0, RoundY, RoundR, Icon::Search, false, chromeFocus == ChromeSearch, view == View::Search ? ui::Blue : 0);
        ui::roundButton(RoundX0 + RoundStep, RoundY, RoundR, Icon::Note, false, chromeFocus == ChromeNow, view == View::NowPlaying ? ui::Blue : 0);
        ui::roundButton(RoundX0 + 2 * RoundStep, RoundY, RoundR, Icon::Gear, true, chromeFocus == ChromeSettings, view == View::Settings ? ui::BlueLight : 0);
    }

    // Tiles are rounded by painting their corners in the colour behind them.
    void drawTrackTile(int trackIndex, float x, float y, float size, u32 bgTop = ui::BottomBg, u32 bgBottom = 0)
    {
        drawTileImage(trackArt(trackIndex), x, y, size, bgTop, bgBottom);
    }

    // A playlist's picture: the first four songs' covers in a 2x2 grid.
    void drawMosaic(const core::Album &a, float x, float y, float size, u32 bgTop = ui::BottomBg, u32 bgBottom = 0)
    {
        const float half = size / 2.0f;
        ui::rect(x, y + 1.0f, size, size, ui::withAlpha(ui::ShadowBase, 0x1E));
        int slot = 0;
        for (int t : a.mosaicTracks) {
            if (slot >= 4) break;
            const float qx = x + (slot % 2) * half;
            const float qy = y + (slot / 2) * half;
            const art::Image *image = trackArt(t);
            if (image != nullptr) art::draw(*image, qx, qy, half, half);
            else ui::rect(qx, qy, half, half, slot % 2 == slot / 2 ? ui::BottomBg : ui::Line);
            ++slot;
        }
        for (; slot < 4; ++slot) {
            ui::rect(x + (slot % 2) * half, y + (slot / 2) * half, half, half, slot % 2 == slot / 2 ? ui::BottomBg : ui::Line);
        }
        ui::roundCorners(x, y, size, size, tileRadius(size), bgTop, bgBottom == 0 ? bgTop : bgBottom);
    }

    static float tileRadius(float size) { return size >= 100.0f ? 12.0f : size >= 40.0f ? 6.0f : size >= 20.0f ? 4.0f : 3.0f; }

    void drawTile(int albumIndex, float x, float y, float size, u32 bgTop = ui::BottomBg, u32 bgBottom = 0)
    {
        if (albumIndex >= 0 && albumIndex < int(library.albums().size()) && library.albums()[albumIndex].isPlaylist) {
            drawMosaic(library.albums()[albumIndex], x, y, size, bgTop, bgBottom);
            return;
        }
        drawTileImage(albumArt(albumIndex), x, y, size, bgTop, bgBottom);
    }

    void drawTileImage(const art::Image *image, float x, float y, float size, u32 bgTop = ui::BottomBg, u32 bgBottom = 0)
    {
        const float r = tileRadius(size);
        ui::roundRect(x, y + 1.0f, size, size, r, ui::withAlpha(ui::ShadowBase, 0x1E));
        if (image != nullptr) {
            art::draw(*image, x, y, size, size);
            ui::roundCorners(x, y, size, size, r, bgTop, bgBottom == 0 ? bgTop : bgBottom);
        } else {
            ui::roundRect(x, y, size, size, r, ui::Sheet);
            ui::icon(Icon::Note, x + size / 2.0f, y + size / 2.0f, size * 0.45f, ui::Line, ui::Sheet);
        }
    }

    void drawRows(const std::vector<Row> &rows, int focus, float y0, int visible, bool numbers = false, bool scrollable = true)
    {
        if (chromeFocus >= 0) focus = -1; // the cursor is up in the top row
        const float px = scrollable ? scrollPx() : 0.0f;
        const int first = int(px / RowStep);
        const float frac = px - first * RowStep;
        ui::clipBegin(0.0f, y0 - 1.0f, BotW, visible * RowStep);
        for (int i = 0; i <= visible; ++i) {
            const int index = first + i;
            if (index >= int(rows.size())) break;
            const Row &r = rows[index];
            const float y = y0 + i * RowStep - frac;
            const bool focused = index == focus;
            if (focused) {
                ui::roundRect(RowX, y + 1.0f, RowW, RowH, 7.0f, ui::withAlpha(ui::BlueDeep, 0x50));
                ui::gradientV(RowX, y, RowW, RowH, ui::BlueLight, ui::BlueDeep);
                ui::roundCorners(RowX, y, RowW, RowH, 7.0f, ui::BottomBg, ui::BottomBg);
            } else {
                ui::roundRect(RowX, y, RowW, RowH, 7.0f, r.dim ? ui::withAlpha(ui::Sheet, 0x80) : ui::Sheet);
            }
            const u32 fg = focused ? ui::White : r.playing ? ui::BlueText : ui::Ink;
            const u32 sub = focused ? ui::withAlpha(ui::White, 0xD8) : ui::Meta;
            float x = RowX + 6.0f;
            const float artSize = RowH - 5.0f;
            if (r.playing) {
                ui::equaliser(x + 4.0f, y + 14.0f, player.level(), focused ? ui::White : ui::Blue, int(frame));
                x += 24.0f;
            } else if (numbers) {
                ui::text(r.sub, x + 16.0f, y + 12.0f, 11.0f, sub, ui::Align::Right);
                x += 24.0f;
            } else if (r.album >= 0 || r.track >= 0) {
                const art::Image *image = r.track >= 0 ? trackArt(r.track) : albumArt(r.album);
                if (image != nullptr) {
                    art::draw(*image, x, y + 2.5f, artSize, artSize);
                    ui::roundCorners(x, y + 2.5f, artSize, artSize, 4.0f, focused ? ui::mix(ui::BlueLight, ui::BlueDeep, 0.1f) : ui::Sheet, focused ? ui::mix(ui::BlueLight, ui::BlueDeep, 0.9f) : ui::Sheet);
                } else {
                    ui::roundRect(x, y + 2.5f, artSize, artSize, 4.0f, ui::BottomBg);
                }
                x += artSize + 6.0f;
            }
            const float rightW = r.right.empty() ? 0.0f : ui::textWidth(r.right, 12.0f) + 8.0f;
            const float titleW = RowW - (x - RowX) - rightW - 8.0f;
            if (numbers || r.sub.empty()) {
                if (focused) ui::marquee("row", r.title, x, y + 10.0f, 14.0f, fg, titleW, ui::Weight::Bold);
                else ui::text(r.title, x, y + 10.0f, 14.0f, fg, ui::Align::Left, titleW, ui::Weight::Bold);
            } else {
                if (focused) ui::marquee("row", r.title, x, y + 4.0f, 13.0f, fg, titleW, ui::Weight::Bold);
                else ui::text(r.title, x, y + 4.0f, 13.0f, fg, ui::Align::Left, titleW, ui::Weight::Bold);
                ui::text(r.sub, x, y + 21.0f, 11.0f, sub, ui::Align::Left, RowW - (x - RowX) - rightW - 8.0f);
            }
            if (!r.right.empty()) ui::text(r.right, RowX + RowW - 8.0f, y + 11.0f, 12.0f, sub, ui::Align::Right);
        }
        ui::clipEnd();
        if (int(rows.size()) > visible) {
            // Scroll hint.
            const float trackH = visible * RowStep - 3.0f;
            const float thumbH = std::max(10.0f, trackH * visible / rows.size());
            const float maxPx = float(int(rows.size()) - visible) * RowStep;
            const float thumbY = y0 + (trackH - thumbH) * (maxPx > 0.0f ? px / maxPx : 0.0f);
            ui::roundRect(RowX + RowW + 3.0f, thumbY, 2.0f, thumbH, 1.0f, ui::Line);
        }
    }

    void drawBottom()
    {
        ui::rect(0, 0, BotW, BotH, ui::BottomBg);
        ui::dots(0, 0, BotW, BotH);
        switch (view) {
        case View::Section: drawBottomSection(); break;
        case View::Album: drawBottomAlbum(); break;
        case View::Artist: drawBottomArtist(); break;
        case View::NowPlaying: drawBottomNowPlaying(); break;
        case View::Search: drawBottomSearch(); break;
        case View::Settings: drawBottomSettings(); break;
        case View::Visualizer: drawBottomVisualizer(); break;
        }
        if (menuOpen) drawMenu();
        if (dialogOpen) drawDialog();
    }

    void drawBottomSection()
    {
        drawBottomChrome(true);
        switch (section) {
        case Section::Home: {
            const int count = int(library.albums().size());
            if (count == 0) {
                if (!scanning) {
                    drawEmptyCard(Icon::Note, "No music yet", "Copy albums to " + settings.musicFolder, "then rescan from Settings.");
                } else if (scanCollected() > 0) {
                    drawEmptyCard(Icon::Note, "Reading your music", std::to_string(scanScanned()) + " of " + std::to_string(scanCollected()) + " files",
                                  "The first time takes a while. After that it is instant.");
                } else {
                    drawEmptyCard(Icon::Note, "Looking for music", "in " + settings.musicFolder);
                }
                break;
            }
            const float px = scrollPx();
            const int firstRow = int(px / TileStep);
            const float frac = px - firstRow * TileStep;
            ui::clipBegin(0.0f, GridY - 4.0f, BotW, GridRows * TileStep + 1.0f);
            for (int r = 0; r <= GridRows; ++r) {
                const float y = GridY + r * TileStep - frac;
                for (int c = 0; c < GridCols; ++c) {
                    const int index = (firstRow + r) * GridCols + c;
                    if (index >= count) break;
                    const float x = GridX + c * TileStep;
                    if (index == focusHome && chromeFocus < 0) {
                        // The cursor: a blue ring around the tile, like the HOME Menu.
                        ui::roundRect(x - 4.0f, y - 4.0f, Tile + 8.0f, Tile + 8.0f, 8.0f, ui::BlueDeep);
                        ui::roundRect(x - 2.0f, y - 2.0f, Tile + 4.0f, Tile + 4.0f, 6.0f, ui::Sheet);
                    }
                    drawTile(index, x, y, Tile);
                    const core::Album &a = library.albums()[index];
                    const bool playingHere = nowTrack >= 0 && library.tracks()[nowTrack].albumKey == a.key;
                    if (playingHere) {
                        // The pill sizes itself to the label.
                        const float labelW = ui::textWidth("Playing", 9.0f, ui::Weight::Bold);
                        const float pillW = 6.0f + 12.0f + 4.0f + labelW + 7.0f;
                        ui::roundRect(x + 4.0f, y + Tile - 18.0f, pillW, 14.0f, 7.0f, ui::DarkPill);
                        ui::equaliser(x + 10.0f, y + Tile - 16.0f, player.level(), ui::PillText, int(frame));
                        ui::text("Playing", x + 4.0f + 6.0f + 12.0f + 4.0f, y + Tile - 15.5f, 9.0f, ui::PillText, ui::Align::Left, 0.0f, ui::Weight::Bold);
                    }
                }
            }
            ui::clipEnd();
            drawSelectionStrip(focusHome);
            break;
        }
        case Section::Songs: drawRows(rowsFor(Section::Songs), focusSongs, GridY, SectionRows); break;
        case Section::Artists: drawRows(rowsFor(Section::Artists), focusArtists, GridY, SectionRows); break;
        case Section::UpNext: {
            const std::vector<Row> rows = rowsFor(Section::UpNext);
            if (rows.empty()) {
                drawEmptyCard(Icon::Queue, "Up next is empty", "Y on any song adds it here.");
            } else {
                drawRows(rows, focusUpNext, GridY, SectionRows);
            }
            break;
        }
        }
    }

    // The details strip for the focused album, fixed under the grid so the
    // tiles never move when focus changes.
    void drawSelectionStrip(int albumIndex)
    {
        if (albumIndex < 0 || albumIndex >= int(library.albums().size())) return;
        const core::Album &a = library.albums()[albumIndex];
        const float y = StripY;
        ui::roundRect(StripX, y + 2.0f, StripW, StripH, 10.0f, ui::withAlpha(ui::BlueDeep, 0x50));
        ui::gradientV(StripX, y, StripW, StripH, ui::BlueLight, ui::BlueDeep);
        ui::roundCorners(StripX, y, StripW, StripH, 10.0f, ui::BottomBg, ui::BottomBg);
        drawTile(albumIndex, StripX + 6.0f, y + 6.0f, StripH - 12.0f, ui::mix(ui::BlueLight, ui::BlueDeep, 0.1f), ui::mix(ui::BlueLight, ui::BlueDeep, 0.9f));
        const float textX = StripX + StripH + 4.0f;
        const float maxW = StripW - (StripH + 4.0f) - 74.0f;
        ui::marquee("block", a.title, textX, y + 5.0f, 14.0f, ui::White, maxW, ui::Weight::Bold);
        ui::icon(Icon::Clock, textX + 5.0f, y + 30.0f, 9.0f, ui::withAlpha(ui::White, 0xE0), ui::BlueDeep);
        std::string meta = minutesText(a.durationSeconds) + " · " + plural(int(a.tracks.size()), "song", "songs") + " · " + (a.isPlaylist ? "Playlist" : a.artist);
        ui::text(meta, textX + 13.0f, y + 24.0f, 10.0f, ui::White, ui::Align::Left, maxW - 13.0f, ui::Weight::Bold);
        const float cy = y + StripH / 2.0f;
        ui::roundButton(StripX + StripW - 54.0f, cy, RoundR, Icon::Play, false);
        ui::roundButton(StripX + StripW - 24.0f, cy, RoundR, Icon::List, false);
    }

    void drawBottomAlbum()
    {
        // The tabs and buttons stay on top; the header card sits under them.
        drawBottomChrome(true);
        if (albumView < 0) return;
        const core::Album &a = library.albums()[albumView];
        ui::roundRect(10.0f, HeaderY + 2.0f, 300.0f, HeaderH, 8.0f, ui::withAlpha(ui::ShadowBase, 0x14));
        ui::roundRect(10.0f, HeaderY, 300.0f, HeaderH, 8.0f, ui::Sheet);
        drawTile(albumView, 16.0f, HeaderY + 6.0f, HeaderH - 12.0f, ui::Sheet);
        ui::text(a.title, 52.0f, HeaderY + 4.0f, 12.0f, ui::Ink, ui::Align::Left, 190.0f, ui::Weight::Bold);
        ui::text((a.isPlaylist ? "Playlist" : a.artist) + (a.year.empty() ? "" : " · " + a.year), 52.0f, HeaderY + 22.0f, 9.0f, ui::Meta, ui::Align::Left, 190.0f);
        ui::roundButton(265.0f, HeaderY + HeaderH / 2.0f, 11.0f, Icon::Shuffle, false);
        ui::roundButton(297.0f, HeaderY + HeaderH / 2.0f, 13.0f, Icon::Play, true);
        drawRows(albumRows(albumView), focusAlbum, HeaderRowsY, HeaderRows, true);
    }

    void drawBottomArtist()
    {
        drawBottomChrome(true);
        const float cy = HeaderY + HeaderH / 2.0f;
        ui::roundRect(10.0f, HeaderY + 2.0f, 300.0f, HeaderH, 8.0f, ui::withAlpha(ui::ShadowBase, 0x14));
        ui::roundRect(10.0f, HeaderY, 300.0f, HeaderH, 8.0f, ui::Sheet);
        ui::circle(30.0f, cy, 14.0f, ui::BottomBg);
        ui::icon(Icon::Person, 30.0f, cy, 15.0f, ui::Meta, ui::BottomBg);
        ui::text(artistView, 52.0f, HeaderY + 4.0f, 12.0f, ui::Ink, ui::Align::Left, 250.0f, ui::Weight::Bold);
        int songs = 0;
        for (int i : artistAlbums) songs += int(library.albums()[i].tracks.size());
        ui::text(plural(int(artistAlbums.size()), "album", "albums") + " · " + plural(songs, "song", "songs"), 52.0f, HeaderY + 22.0f, 9.0f, ui::Meta);
        drawRows(artistRows(), focusArtist, HeaderRowsY, HeaderRows);
    }

    static constexpr float TransportX[5] = {92.0f, 126.0f, 160.0f, 194.0f, 228.0f};
    static constexpr float TransportR[5] = {10.0f, 13.0f, 17.0f, 13.0f, 10.0f};

    void drawBottomNowPlaying()
    {
        drawBottomChrome(true);
        const bool playing = player.playing();
        // The cursor is here only when it is neither in the queue nor in the top row.
        const int ring = focusNowQueue < 0 && chromeFocus < 0 ? focusNow : -1;
        ui::roundButton(TransportX[0], NowButtonsY, TransportR[0], Icon::Shuffle, false, ring == 0);
        if (settings.shuffle) ui::circle(TransportX[0], NowButtonsY + 14.0f, 1.8f, ui::Blue);
        ui::roundButton(TransportX[1], NowButtonsY, TransportR[1], Icon::Prev, false, ring == 1);
        ui::roundButton(TransportX[2], NowButtonsY, TransportR[2], playing ? Icon::Pause : Icon::Play, true, ring == 2);
        ui::roundButton(TransportX[3], NowButtonsY, TransportR[3], Icon::Next, false, ring == 3);
        ui::roundButton(TransportX[4], NowButtonsY, TransportR[4], settings.repeat == 2 ? Icon::RepeatOne : Icon::Repeat, false, ring == 4);
        if (settings.repeat != 0) ui::circle(TransportX[4], NowButtonsY + 14.0f, 1.8f, ui::Blue);
        // A seek bar with the times at its ends; the song itself is on the
        // top screen.
        if (nowTrack >= 0) {
            const core::Track &t = library.tracks()[nowTrack];
            const float pos = float(player.positionSeconds());
            ui::text(core::formatClock(int(pos)), 36.0f, NowSeekY - 6.0f, 9.0f, ui::Meta, ui::Align::Right);
            ui::progressBar(40.0f, NowSeekY - 2.0f, 240.0f, 5.0f, t.durationSeconds > 0 ? pos / t.durationSeconds : 0.0f);
            ui::text(t.durationSeconds > 0 ? core::formatClock(t.durationSeconds) : "", 284.0f, NowSeekY - 6.0f, 9.0f, ui::Meta, ui::Align::Left);
        }
        const std::vector<const core::Item *> upcoming = queue.upcoming();
        if (upcoming.size() > 1) {
            ui::text("Up next · " + plural(int(upcoming.size()) - 1, "song", "songs"), 14.0f, NowQueueLabelY, 9.0f, ui::Meta);
            std::vector<Row> rows;
            for (size_t i = 1; i < upcoming.size() && int(rows.size()) < NowQueueRows; ++i) {
                Row r;
                r.title = upcoming[i]->title;
                r.sub = upcoming[i]->artist;
                r.right = upcoming[i]->durationSeconds > 0 ? core::formatClock(upcoming[i]->durationSeconds) : "";
                r.album = albumIndexOfTrack(upcoming[i]->libraryIndex);
                r.track = upcoming[i]->libraryIndex;
                rows.push_back(r);
            }
            drawRows(rows, focusNowQueue - 1, NowQueueY, NowQueueRows, false, false);
        } else {
            ui::text("Nothing queued after this song.", 14.0f, NowQueueLabelY, 9.0f, ui::Meta);
        }
    }

    void drawBottomSearch()
    {
        ui::roundRect(10.0f, 10.0f, 300.0f, SearchBarH, SearchBarH / 2.0f, ui::withAlpha(ui::BlueDeep, 0x60));
        ui::roundRect(11.5f, 11.5f, 297.0f, SearchBarH - 3.0f, (SearchBarH - 3.0f) / 2.0f, ui::Sheet);
        ui::icon(Icon::Search, 27.0f, 10.0f + SearchBarH / 2.0f, 12.0f, ui::Meta, ui::Sheet);
        if (searchQuery.empty()) ui::text("Song, artist or album", 40.0f, 19.0f, 9.0f, ui::Meta);
        else ui::text(searchQuery, 40.0f, 18.0f, 11.0f, ui::Ink, ui::Align::Left, 228.0f);
        ui::icon(Icon::Keyboard, 294.0f, 10.0f + SearchBarH / 2.0f, 13.0f, ui::Meta, ui::Sheet);
        if (!searchResults.empty()) {
            drawRows(searchRows(), focusSearch, SearchRowsY, SearchRows);
        } else if (!searchQuery.empty()) {
            ui::text("No matches for \"" + searchQuery + "\"", 160.0f, 120.0f, 11.0f, ui::Meta, ui::Align::Center);
        }
    }

    void drawBottomSettings()
    {
        const std::vector<SettingRow> rows = settingRows();
        const float totalH = settingsHeight(rows);
        const std::vector<float> tops = settingTops(rows);
        const float sheetY = 8.0f - settingsScrollPx;
        ui::roundRect(10.0f, sheetY + 2.0f, 300.0f, totalH, 8.0f, ui::withAlpha(ui::ShadowBase, 0x14));
        ui::roundRect(10.0f, sheetY, 300.0f, totalH, 8.0f, ui::Sheet);
        for (int i = 0; i < int(rows.size()); ++i) {
            const SettingRow &r = rows[i];
            const float y = tops[i];
            if (y > BotH || y + SettingRowH < 0.0f) continue;
            if (r.header) {
                ui::text(r.label, 18.0f, y + 3.0f, 9.0f, ui::Meta);
                continue;
            }
            const bool focused = i == focusSettings;
            if (focused) ui::roundRect(14.0f, y, 292.0f, SettingRowH, 5.0f, ui::BlueTint);
            const u32 fg = !r.enabled ? ui::Meta : focused ? ui::BlueText : ui::Ink;
            ui::text(r.label, 20.0f, y + 5.0f, 11.0f, fg, ui::Align::Left, 190.0f);
            if (r.toggle >= 0) {
                ui::toggle(278.0f, y + 5.0f, r.toggle == 1);
            } else {
                if (!r.value.empty()) ui::text(r.value, 288.0f, y + 6.0f, 9.0f, focused ? ui::BlueText : ui::Meta, ui::Align::Right, 100.0f);
                if (r.enabled) ui::icon(Icon::ChevronRight, 298.0f, y + SettingRowH / 2.0f, 9.0f, focused ? ui::BlueText : ui::Meta, ui::Sheet);
            }
        }
    }

    void drawBottomVisualizer()
    {
        const float dimAmount = visualizerDim > 300 ? std::min(1.0f, (visualizerDim - 300) / 120.0f) : 0.0f;
        ui::rect(0, 0, BotW, BotH, ui::withAlpha(ui::ShadowBase, 0x80));
        static const char *names[4] = {"Bars", "Wave", "Dots", "Art only"};
        ui::roundRect(30.0f, 70.0f, 260.0f, 32.0f, 8.0f, ui::Sheet);
        for (int i = 0; i < 4; ++i) {
            const float x = 34.0f + i * 63.0f;
            if (settings.visualizerStyle == i) ui::roundRect(x, 74.0f, 61.0f, 24.0f, 5.0f, ui::BlueTint);
            ui::text(names[i], x + 30.5f, 80.0f, 10.0f, settings.visualizerStyle == i ? ui::BlueText : ui::Ink, ui::Align::Center);
        }
        drawMiniPlayerAt(30.0f, 110.0f, 260.0f);
        ui::roundButton(160.0f, 168.0f, 16.0f, player.playing() ? Icon::Pause : Icon::Play, true);
        ui::text("Screen dims after a while", 160.0f, 222.0f, 9.0f, ui::withAlpha(ui::White, 0xA0), ui::Align::Center);
        if (dimAmount > 0.0f) ui::rect(0, 0, BotW, BotH, ui::withAlpha(C2D_Color32(0, 0, 0, 0xFF), u8(0xE6 * dimAmount)));
    }

    void drawMiniPlayerAt(float x, float y, float w)
    {
        ui::roundRect(x, y, w, 38.0f, 8.0f, ui::Sheet);
        if (nowTrack < 0) {
            ui::text("Nothing playing", x + 12.0f, y + 12.0f, 11.0f, ui::Meta);
            return;
        }
        const core::Track &t = library.tracks()[nowTrack];
        drawTrackTile(nowTrack, x + 6.0f, y + 6.0f, 26.0f, ui::Sheet);
        ui::text(t.title, x + 38.0f, y + 4.0f, 11.0f, ui::Ink, ui::Align::Left, w - 50.0f, ui::Weight::Bold);
        ui::text(t.artist, x + 38.0f, y + 19.0f, 9.0f, ui::Meta, ui::Align::Left, w - 50.0f);
        const float pos = float(player.positionSeconds());
        ui::progressBar(x + 38.0f, y + 31.0f, w - 50.0f, 3.0f, t.durationSeconds > 0 ? pos / t.durationSeconds : 0.0f);
    }

    void drawMenu()
    {
        ui::rect(0, 0, BotW, BotH, ui::Dim);
        const int count = int(menu.size());
        const float h = 8.0f + count * MenuRowStep;
        ui::roundRect(MenuX, MenuY + 2.0f, MenuW, h, 8.0f, ui::withAlpha(ui::ShadowBase, 0x30));
        ui::roundRect(MenuX, MenuY, MenuW, h, 8.0f, ui::Sheet);
        for (int i = 0; i < count; ++i) {
            const float y = MenuY + 4.0f + i * MenuRowStep;
            const bool focused = i == menuFocus;
            if (focused) ui::roundRect(MenuX + 4.0f, y, MenuW - 8.0f, MenuRowH, 5.0f, ui::BlueTint);
            const u32 fg = !menu[i].enabled ? ui::Meta : focused ? ui::BlueText : ui::Ink;
            ui::text(menu[i].label, MenuX + 10.0f, y + 8.0f, 12.0f, fg, ui::Align::Left, MenuW - 50.0f);
            ui::icon(menu[i].icon, MenuX + MenuW - 15.0f, y + MenuRowH / 2.0f, 12.0f, focused ? ui::BlueText : ui::Meta, focused ? ui::BlueTint : ui::Sheet);
        }
        ui::roundButton(MenuX + MenuW + 20.0f, MenuY + 12.0f, 11.0f, Icon::Close, true);
    }

    void drawDialog()
    {
        ui::rect(0, 0, BotW, BotH, ui::Dim);
        ui::roundRect(30.0f, 62.0f, 260.0f, 116.0f, 8.0f, ui::withAlpha(ui::ShadowBase, 0x30));
        ui::roundRect(30.0f, 60.0f, 260.0f, 116.0f, 8.0f, ui::Sheet);
        ui::text(dialogTitle, 42.0f, 68.0f, 14.0f, ui::Ink, ui::Align::Left, 236.0f, ui::Weight::Bold);
        // Wrap body.
        std::string line;
        std::string word;
        float y = 90.0f;
        for (size_t i = 0; i <= dialogBody.size(); ++i) {
            const char c = i < dialogBody.size() ? dialogBody[i] : ' ';
            if (c == ' ') {
                const std::string candidate = line.empty() ? word : line + " " + word;
                if (ui::textWidth(candidate, 10.0f) > 236.0f && !line.empty()) {
                    ui::text(line, 42.0f, y, 10.0f, ui::InkSoft);
                    y += 13.0f;
                    line = word;
                } else {
                    line = candidate;
                }
                word.clear();
            } else {
                word += c;
            }
        }
        if (!line.empty()) ui::text(line, 42.0f, y, 10.0f, ui::InkSoft);
        ui::roundRect(170.0f, DialogButtonY, 70.0f, DialogButtonH, 6.0f, ui::BottomBg);
        ui::text(dialogSecondary, 205.0f, DialogButtonY + 7.0f, 10.0f, ui::Ink, ui::Align::Center);
        ui::roundRect(246.0f, DialogButtonY, 64.0f, DialogButtonH, 6.0f, ui::ButtonDark);
        ui::text(dialogPrimary, 278.0f, DialogButtonY + 7.0f, 10.0f, ui::ButtonDarkIcon, ui::Align::Center);
        ui::keyCap("B", 180.0f, DialogButtonY + 12.0f, ui::Meta);
        ui::keyCap("A", 256.0f, DialogButtonY + 12.0f, ui::Meta);
    }

    // ---- lid handling ----
    void updateLid()
    {
        if (frame % 15 != 0) return;
        // While music plays the console must not sleep when the lid closes;
        // the screens go dark instead and come back when it opens.
        const bool keepAwake = settings.keepPlayingLidClosed && player.playing();
        aptSetSleepAllowed(!keepAwake);
        if (keepAwake) {
            platform::setBacklights(!platform::lidClosed());
        } else {
            platform::setBacklights(true);
        }
    }
};

App::App() : s_(new State)
{
    s_->settings.load();
    ui::setTheme(s_->settings.darkMode);
    s_->audioReady = audio::init(s_->audioError);
    if (!s_->audioReady) platform::log("audio: %s", s_->audioError.c_str());
    s_->applyQueueModes();
    art::startLoader();
    s_->refreshStatus();
    aptHook(&s_->aptCookie, State::aptHookEntry, s_);
    s_->startWatchdog();
    s_->startScan();
}

App::~App()
{
    aptUnhook(&s_->aptCookie);
    s_->stopWatchdog();
    s_->player.stop();
    s_->finishScanThread();
    art::stopLoader();
    for (auto &entry : s_->art) art::release(entry.second);
    audio::shutdown();
    delete s_;
}

void App::run()
{
    C3D_RenderTarget *top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget *bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    // Frame timing, logged a few times at ten-second intervals, to catch
    // anything that drags. The "top" figure includes the vsync wait.
    u64 tUpdate = 0;
    u64 tTop = 0;
    u64 tBottom = 0;
    u64 tEnd = 0;
    u64 tWait = 0;
    int timed = 0;
    int timingLogs = 0;
    u64 last = svcGetSystemTick();
    u64 hostLast = osGetTime();
    while (aptMainLoop()) {
        const u64 t0 = svcGetSystemTick();
        tWait += t0 - last;
        s_->update();
        if (hidKeysDown() & KEY_START && (hidKeysHeld() & KEY_SELECT)) break;
        s_->updateLid();
        const u64 t1 = svcGetSystemTick();

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        ui::beginFrame();
        ui::marqueeTick();
        C2D_TargetClear(top, ui::TopBg);
        C2D_SceneBegin(top);
        ui::beginScreen(TopW, TopH);
        s_->drawTop();
        const u64 t2 = svcGetSystemTick();
        C2D_TargetClear(bottom, ui::BottomBg);
        C2D_SceneBegin(bottom);
        ui::beginScreen(BotW, BotH);
        s_->drawBottom();
        const u64 t3 = svcGetSystemTick();
        C3D_FrameEnd(0);
        const u64 t4 = svcGetSystemTick();
        last = t4;
        tUpdate += t1 - t0;
        tTop += t2 - t1;
        tBottom += t3 - t2;
        tEnd += t4 - t3;
        if (++timed == 600 && timingLogs < 6) {
            ++timingLogs;
            const double ms = 1000.0 / double(SYSCLOCK_ARM11) / 600.0;
            platform::log("frame ms: update %.2f top %.2f bottom %.2f end %.2f wait %.2f | host ms per frame %.1f", tUpdate * ms, tTop * ms, tBottom * ms, tEnd * ms, tWait * ms, double(osGetTime() - hostLast) / 600.0);
            hostLast = osGetTime();
            tUpdate = tTop = tBottom = tEnd = tWait = 0;
            timed = 0;
        }
    }
}
