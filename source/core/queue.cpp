#include "queue.hpp"

#include <algorithm>
#include <numeric>

namespace core {

namespace {

// A small deterministic generator so a shuffle can be reproduced in tests and
// restored from a saved seed.
struct Xorshift {
    uint32_t state;
    uint32_t next()
    {
        uint32_t x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state = x;
        return x;
    }
};

} // namespace

void PlayQueue::add(const Item &track)
{
    tracks_.push_back(track);
    if (shuffle_) {
        // New tracks join the end of the shuffled order rather than
        // reshuffling what the listener can already see.
        order_.push_back(tracks_.size() - 1);
    }
}

void PlayQueue::clear()
{
    tracks_.clear();
    order_.clear();
    cursor_ = 0;
}

const Item *PlayQueue::current() const
{
    if (tracks_.empty()) {
        return nullptr;
    }
    return &tracks_[cursor_];
}

bool PlayQueue::jumpTo(std::size_t index)
{
    if (index >= tracks_.size()) {
        return false;
    }
    cursor_ = index;
    return true;
}

std::size_t PlayQueue::orderIndex() const
{
    if (!shuffle_) {
        return cursor_;
    }
    const auto found = std::find(order_.begin(), order_.end(), cursor_);
    return found == order_.end() ? 0 : static_cast<std::size_t>(found - order_.begin());
}

bool PlayQueue::next(bool userSkip)
{
    if (tracks_.empty()) {
        return false;
    }
    if (repeat_ == Repeat::One && !userSkip) {
        return true;
    }
    const std::size_t index = orderIndex();
    const bool atEnd = index + 1 >= tracks_.size();
    if (atEnd && repeat_ == Repeat::Off) {
        return false;
    }
    const std::size_t nextIndex = atEnd ? 0 : index + 1;
    cursor_ = shuffle_ ? order_[nextIndex] : nextIndex;
    return true;
}

bool PlayQueue::previous()
{
    if (tracks_.empty()) {
        return false;
    }
    const std::size_t index = orderIndex();
    if (index == 0) {
        if (repeat_ == Repeat::Off) {
            return false;
        }
        const std::size_t last = tracks_.size() - 1;
        cursor_ = shuffle_ ? order_[last] : last;
        return true;
    }
    cursor_ = shuffle_ ? order_[index - 1] : index - 1;
    return true;
}

void PlayQueue::setShuffle(bool enabled, uint32_t seed)
{
    shuffle_ = enabled;
    order_.clear();
    if (!enabled || tracks_.empty()) {
        return;
    }

    order_.resize(tracks_.size());
    std::iota(order_.begin(), order_.end(), 0);

    // Fisher-Yates over everything except the current track, which stays at
    // the front so the shuffle starts from what is playing.
    std::swap(order_[0], order_[cursor_]);
    Xorshift random{seed == 0 ? 0x9E3779B9u : seed};
    for (std::size_t i = order_.size() - 1; i > 1; --i) {
        const std::size_t j = 1 + random.next() % i;
        std::swap(order_[i], order_[j]);
    }
}

std::vector<const Item *> PlayQueue::upcoming() const
{
    std::vector<const Item *> result;
    if (tracks_.empty()) {
        return result;
    }
    const std::size_t start = orderIndex();
    for (std::size_t i = start; i < tracks_.size(); ++i) {
        result.push_back(&tracks_[shuffle_ ? order_[i] : i]);
    }
    return result;
}

} // namespace core

namespace core {

bool PlayQueue::removeAt(std::size_t index)
{
    if (index >= tracks_.size()) return false;
    tracks_.erase(tracks_.begin() + static_cast<std::ptrdiff_t>(index));
    std::size_t removedSlot = 0;
    if (shuffle_) {
        for (std::size_t i = 0; i < order_.size(); ++i) {
            if (order_[i] == index) {
                removedSlot = i;
                order_.erase(order_.begin() + static_cast<std::ptrdiff_t>(i));
                break;
            }
        }
        for (std::size_t &o : order_) {
            if (o > index) --o;
        }
    }
    if (tracks_.empty()) {
        cursor_ = 0;
        return true;
    }
    if (cursor_ > index) {
        --cursor_;
    } else if (cursor_ == index) {
        if (shuffle_) {
            cursor_ = order_[removedSlot < order_.size() ? removedSlot : order_.size() - 1];
        } else if (cursor_ >= tracks_.size()) {
            cursor_ = tracks_.size() - 1;
        }
    }
    return true;
}

} // namespace core
