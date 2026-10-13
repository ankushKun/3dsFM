#include "platform.hpp"

#include <3ds.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <sys/stat.h>

namespace platform {

namespace {

const char *AppDir = "sdmc:/3ds/3dsFM";
bool ptmReady = false;
bool lcdReady = false;
bool backlightsOn = true;

struct FileSource : core::ByteSource {
    FILE *file = nullptr;
    uint64_t length = 0;
    explicit FileSource(const std::string &path)
    {
        file = std::fopen(path.c_str(), "rb");
        if (file != nullptr) {
            // The default 1 KB stdio buffer turns every tag read into dozens
            // of trips to the FS service; a bigger one makes a scan of a
            // large library several times faster.
            std::setvbuf(file, nullptr, _IOFBF, 32 * 1024);
            std::fseek(file, 0, SEEK_END);
            length = uint64_t(std::ftell(file));
        }
    }
    ~FileSource() override
    {
        if (file != nullptr) std::fclose(file);
    }
    size_t read(uint64_t offset, void *buffer, size_t n) override
    {
        if (file == nullptr || offset >= length) return 0;
        if (std::fseek(file, long(offset), SEEK_SET) != 0) return 0;
        return std::fread(buffer, 1, n, file);
    }
    uint64_t size() override { return length; }
};

std::string trim(const std::string &s)
{
    size_t a = 0;
    size_t b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
    return s.substr(a, b - a);
}

} // namespace

void init()
{
    makeDirectories(AppDir);
    ptmReady = R_SUCCEEDED(ptmuInit());
    // The log survives a restart so a freeze or crash leaves its last lines
    // behind; it starts over once it grows past a quarter megabyte.
    const std::string logPath = std::string(AppDir) + "/log.txt";
    struct stat st {};
    if (stat(logPath.c_str(), &st) == 0 && st.st_size > 256 * 1024) std::remove(logPath.c_str());
    const bool newModel = []() {
        bool n3ds = false;
        return R_SUCCEEDED(APT_CheckNew3DS(&n3ds)) && n3ds;
    }();
    log("---- 3dsFM %s started (%s, %s)", APP_VERSION, newModel ? "New 3DS" : "Old 3DS", envIsHomebrew() ? "homebrew launcher" : "title");
}

void shutdown()
{
    if (lcdReady) {
        if (!backlightsOn) GSPLCD_PowerOnAllBacklights();
        gspLcdExit();
    }
    if (ptmReady) ptmuExit();
}

std::vector<core::Entry> listDirectory(const std::string &path)
{
    std::vector<core::Entry> out;
    DIR *dir = opendir(path.c_str());
    if (dir == nullptr) return out;
    while (dirent *e = readdir(dir)) {
        if (e->d_name[0] == '\0') continue;
        if (std::strcmp(e->d_name, ".") == 0 || std::strcmp(e->d_name, "..") == 0) continue;
        core::Entry entry;
        entry.name = e->d_name;
        if (e->d_type == DT_DIR) {
            entry.isDirectory = true;
        } else if (e->d_type == DT_UNKNOWN) {
            struct stat st {};
            entry.isDirectory = stat((path + "/" + entry.name).c_str(), &st) == 0 && S_ISDIR(st.st_mode);
        }
        out.push_back(std::move(entry));
    }
    closedir(dir);
    return out;
}

bool readTagsFromFile(const std::string &path, core::Format format, core::Tags &out)
{
    FileSource source(path);
    if (source.file == nullptr) return false;
    return core::readTags(source, format, out);
}

bool readPictureFromFile(const std::string &path, core::Format format, core::Picture &out)
{
    FileSource source(path);
    if (source.file == nullptr) return false;
    return core::readPicture(source, format, out);
}

bool readWholeFile(const std::string &path, std::vector<uint8_t> &out, size_t maxBytes)
{
    FileSource source(path);
    if (source.file == nullptr || source.length > maxBytes) return false;
    out.resize(size_t(source.length));
    return source.read(0, out.data(), out.size()) == out.size();
}

bool readTextFile(const std::string &path, std::string &out)
{
    std::vector<uint8_t> bytes;
    if (!readWholeFile(path, bytes, 16 * 1024 * 1024)) return false;
    out.assign(bytes.begin(), bytes.end());
    return true;
}

bool writeTextFile(const std::string &path, const std::string &text)
{
    // Written straight to its name. An earlier version wrote a temporary
    // file and renamed it over the target; the index then never seemed to
    // survive a restart on a console, while plain writes (the log, the
    // settings) did. The readers tolerate a torn line, so a power cut in
    // the middle costs at most the tail of the file.
    FILE *f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) {
        log("write failed: could not create %s", path.c_str());
        return false;
    }
    const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    std::fclose(f);
    if (!ok) {
        log("write failed: short write to %s", path.c_str());
        std::remove(path.c_str());
        return false;
    }
    return true;
}

bool appendTextFile(const std::string &path, const std::string &text)
{
    FILE *f = std::fopen(path.c_str(), "ab");
    if (f == nullptr) {
        log("append failed: could not open %s", path.c_str());
        return false;
    }
    const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    std::fclose(f);
    if (!ok) log("append failed: short write to %s", path.c_str());
    return ok;
}

bool fileExists(const std::string &path)
{
    struct stat st {};
    return stat(path.c_str(), &st) == 0;
}

void makeDirectories(const std::string &path)
{
    std::string current;
    for (size_t i = 0; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == '/') {
            current = path.substr(0, i);
            if (!current.empty() && current.back() != ':') mkdir(current.c_str(), 0777);
        }
    }
}

std::string clockText()
{
    const time_t now = time(nullptr);
    struct tm *t = localtime(&now);
    if (t == nullptr) return "";
    int hour = t->tm_hour % 12;
    if (hour == 0) hour = 12;
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%d:%02d %s", hour, t->tm_min, t->tm_hour < 12 ? "AM" : "PM");
    return buffer;
}

std::string dateText()
{
    const time_t now = time(nullptr);
    struct tm *t = localtime(&now);
    if (t == nullptr) return "";
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%d/%d", t->tm_mon + 1, t->tm_mday);
    return buffer;
}

int batteryLevel()
{
    u8 level = 5;
    if (ptmReady) PTMU_GetBatteryLevel(&level);
    return level;
}

bool charging()
{
    u8 state = 0;
    if (ptmReady) PTMU_GetBatteryChargeState(&state);
    return state != 0;
}

int wifiStrength()
{
    return osGetWifiStrength();
}

bool headphonesPlugged()
{
    bool plugged = false;
    DSP_GetHeadphoneStatus(&plugged);
    return plugged;
}

bool lidClosed()
{
    u8 state = 1;
    if (ptmReady) PTMU_GetShellState(&state);
    return state == 0;
}

void setBacklights(bool on)
{
    if (on == backlightsOn) return;
    // The gsp::Lcd service is only opened the first time the backlights
    // have to go off (lid closed with music playing), so the app holds no
    // session on it the rest of the time.
    if (!lcdReady) {
        if (on) return;
        lcdReady = R_SUCCEEDED(gspLcdInit());
        if (!lcdReady) return;
    }
    if (on) GSPLCD_PowerOnAllBacklights(); else GSPLCD_PowerOffAllBacklights();
    backlightsOn = on;
}

int64_t sdFreeBytes()
{
    FS_ArchiveResource res{};
    if (R_FAILED(FSUSER_GetArchiveResource(&res, SYSTEM_MEDIATYPE_SD))) return -1;
    return int64_t(res.freeClusters) * res.clusterSize;
}

int64_t sdTotalBytes()
{
    FS_ArchiveResource res{};
    if (R_FAILED(FSUSER_GetArchiveResource(&res, SYSTEM_MEDIATYPE_SD))) return -1;
    return int64_t(res.totalClusters) * res.clusterSize;
}

bool isNew3ds()
{
    bool result = false;
    APT_CheckNew3DS(&result);
    return result;
}

bool keyboard(const std::string &hint, const std::string &initial, std::string &out)
{
    SwkbdState state;
    swkbdInit(&state, SWKBD_TYPE_NORMAL, 2, 120);
    swkbdSetHintText(&state, hint.c_str());
    swkbdSetInitialText(&state, initial.c_str());
    swkbdSetFeatures(&state, SWKBD_PREDICTIVE_INPUT);
    swkbdSetButton(&state, SWKBD_BUTTON_LEFT, "Cancel", false);
    swkbdSetButton(&state, SWKBD_BUTTON_RIGHT, "OK", true);
    char buffer[256] = {};
    const SwkbdButton button = swkbdInputText(&state, buffer, sizeof(buffer));
    if (button != SWKBD_BUTTON_RIGHT) return false;
    out = buffer;
    return true;
}

void log(const char *format, ...)
{
    FILE *f = std::fopen((std::string(AppDir) + "/log.txt").c_str(), "a");
    if (f == nullptr) return;
    va_list args;
    va_start(args, format);
    std::vfprintf(f, format, args);
    va_end(args);
    std::fputc('\n', f);
    std::fclose(f);
}

void Settings::load()
{
    FILE *f = std::fopen((std::string(AppDir) + "/settings.ini").c_str(), "r");
    if (f == nullptr) return;
    char line[512];
    while (std::fgets(line, sizeof(line), f) != nullptr) {
        const std::string text = trim(line);
        const size_t eq = text.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = trim(text.substr(0, eq));
        const std::string value = trim(text.substr(eq + 1));
        if (key == "music_folder" && !value.empty()) musicFolder = value;
        else if (key == "keep_playing_lid_closed") keepPlayingLidClosed = value == "1";
        else if (key == "art_on_top_screen") artOnTopScreen = value == "1";
        else if (key == "dark_mode") darkMode = value == "1";
        else if (key == "shuffle") shuffle = value == "1";
        else if (key == "repeat") repeat = std::atoi(value.c_str());
        else if (key == "visualizer_style") visualizerStyle = std::atoi(value.c_str());
        else if (key == "songs_view") songsView = std::max(0, std::min(2, std::atoi(value.c_str())));
        else if (key == "second_core") secondCore = value == "1";
    }
    std::fclose(f);
}

void Settings::save() const
{
    FILE *f = std::fopen((std::string(AppDir) + "/settings.ini").c_str(), "w");
    if (f == nullptr) return;
    std::fprintf(f, "music_folder=%s\nkeep_playing_lid_closed=%d\nart_on_top_screen=%d\ndark_mode=%d\nshuffle=%d\nrepeat=%d\nvisualizer_style=%d\nsongs_view=%d\nsecond_core=%d\n",
                 musicFolder.c_str(), keepPlayingLidClosed ? 1 : 0, artOnTopScreen ? 1 : 0, darkMode ? 1 : 0, shuffle ? 1 : 0, repeat, visualizerStyle, songsView, secondCore ? 1 : 0);
    std::fclose(f);
}

} // namespace platform
