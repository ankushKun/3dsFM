#pragma once

#include "tags.hpp"

#include <cstdint>
#include <string>

// NDSP playback fed by a decoder thread. One track at a time; the app's
// queue decides what comes next when `finished()` reports true.
namespace audio {

bool init(std::string &error);
void shutdown();
bool ready();
// Whether new decode threads may go on the second core. Off, they stay on
// the app core; used when the second core stops delivering after a HOME
// Menu visit.
void setSecondCoreAllowed(bool allowed);
bool secondCoreAllowed();

class Player {
public:
    Player();
    ~Player();

    bool play(const std::string &path, core::Format format, std::string &error);
    void stop();
    void pause();
    void resume();
    void toggle();
    bool playing() const;  // actively producing sound
    bool loaded() const;   // a track is open (playing or paused)
    bool finished();       // true once, when the track ran out

    double positionSeconds() const;
    void seek(double seconds);

    // Recent mono samples for the visualizer; writes up to `count` values
    // scaled to -1..1 and returns how many were written.
    int recentSamples(float *out, int count) const;
    // Peak level 0..1 of the last buffer, for the little equaliser glyph.
    float level() const;

private:
    struct Impl;
    Impl *impl_;
};

} // namespace audio
