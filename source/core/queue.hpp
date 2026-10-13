#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace core {

struct Item {
    int libraryIndex = -1;
    std::string id;
    std::string title;
    std::string artist;
    std::string album;
    int durationSeconds = 0;
};

enum class Repeat { Off, All, One };

// The play queue: an ordered list of tracks plus a cursor. Shuffle keeps the
// list intact and plays through a separate order, so turning it off returns
// to the original sequence with the same track still current.
class PlayQueue {
public:
    void add(const Item &track);
    void clear();

    std::size_t size() const { return tracks_.size(); }
    bool empty() const { return tracks_.empty(); }

    // The track under the cursor, or nullptr when the queue is empty.
    const Item *current() const;
    std::size_t position() const { return cursor_; }
    bool jumpTo(std::size_t index);
    // Removes a track by its index in the queue. Removing the current track
    // moves the cursor to the track that would have played next.
    bool removeAt(std::size_t index);

    // Advances the cursor. userSkip means the listener pressed next, which
    // moves on even in repeat-one. Returns false at the end of the queue when
    // repeat is off, leaving the cursor where it was.
    bool next(bool userSkip);
    bool previous();

    void setRepeat(Repeat repeat) { repeat_ = repeat; }
    Repeat repeat() const { return repeat_; }

    void setShuffle(bool enabled, uint32_t seed);
    bool shuffled() const { return shuffle_; }

    // The tracks in playback order, starting from the current one.
    std::vector<const Item *> upcoming() const;

    // Points every item at its track in a rebuilt library. An item whose
    // track is gone (newIndex returns -1) leaves the queue.
    void remap(const std::function<int(const Item &)> &newIndex);

private:
    std::size_t orderIndex() const;

    std::vector<Item> tracks_;
    std::vector<std::size_t> order_;
    std::size_t cursor_ = 0;
    Repeat repeat_ = Repeat::Off;
    bool shuffle_ = false;
};

} // namespace core
