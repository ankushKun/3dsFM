#pragma once

#include "library.hpp"
#include "tags.hpp"

#include <cstdint>
#include <string>
#include <vector>

// Console services the app needs, kept in one place.
namespace platform {

void init();
void shutdown();

// Directory listing and tag reading for the library scanner.
std::vector<core::Entry> listDirectory(const std::string &path);
bool readTagsFromFile(const std::string &path, core::Format format, core::Tags &out);
bool readPictureFromFile(const std::string &path, core::Format format, core::Picture &out);
bool readWholeFile(const std::string &path, std::vector<uint8_t> &out, size_t maxBytes);
bool readTextFile(const std::string &path, std::string &out);
bool writeTextFile(const std::string &path, const std::string &text);
bool appendTextFile(const std::string &path, const std::string &text);
// The library index, see core::encodeIndex.
constexpr const char *IndexPath = "sdmc:/3ds/3dsFM/library.idx";
bool fileExists(const std::string &path);
void makeDirectories(const std::string &path);

// Clock and status.
std::string clockText();   // "8:25 PM"
std::string dateText();    // "10/12"
int batteryLevel();        // 0..5
bool charging();
int wifiStrength();        // 0..3
bool headphonesPlugged();
bool lidClosed();
void setBacklights(bool on);
int64_t sdFreeBytes();
int64_t sdTotalBytes();
bool isNew3ds();

// System keyboard. Returns false when cancelled.
bool keyboard(const std::string &hint, const std::string &initial, std::string &out);

// Debug log at sdmc:/3ds/3dsFM/log.txt.
void log(const char *format, ...);

struct Settings {
    std::string musicFolder = "sdmc:/Music";
    bool keepPlayingLidClosed = true;
    bool artOnTopScreen = true;
    bool darkMode = true;
    bool shuffle = false;
    int repeat = 0; // 0 off, 1 all, 2 one
    int visualizerStyle = 0;
    int songsView = 0;       // 0 cover flow, 1 grid, 2 list
    bool secondCore = false; // decode on the second core (New 3DS)
    void load();
    void save() const;
};

} // namespace platform
