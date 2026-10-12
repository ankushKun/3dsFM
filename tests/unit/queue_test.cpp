#include "../check.hpp"
#include "queue.hpp"

#include <set>
#include <string>

using core::PlayQueue;
using core::Repeat;
using core::Item;

namespace {

PlayQueue fourTracks()
{
    PlayQueue queue;
    for (int i = 1; i <= 4; ++i) {
        const std::string id = "t" + std::to_string(i);
        queue.add({i - 1, id, "Item " + std::to_string(i), "Artist", "Album", 60 * i});
    }
    return queue;
}

std::string currentId(const PlayQueue &queue)
{
    return queue.current() ? queue.current()->id : "";
}

} // namespace

TEST("an empty queue has no current track and cannot move")
{
    PlayQueue queue;
    CHECK(queue.current() == nullptr);
    CHECK(!queue.next(true));
    CHECK(!queue.previous());
    CHECK(queue.upcoming().empty());
}

TEST("next and previous walk the queue in order")
{
    PlayQueue queue = fourTracks();
    CHECK_EQ(currentId(queue), std::string("t1"));
    CHECK(queue.next(true));
    CHECK_EQ(currentId(queue), std::string("t2"));
    CHECK(queue.next(false));
    CHECK_EQ(currentId(queue), std::string("t3"));
    CHECK(queue.previous());
    CHECK_EQ(currentId(queue), std::string("t2"));
}

TEST("repeat off stops at both ends")
{
    PlayQueue queue = fourTracks();
    CHECK(!queue.previous());
    CHECK_EQ(currentId(queue), std::string("t1"));
    queue.jumpTo(3);
    CHECK(!queue.next(false));
    CHECK_EQ(currentId(queue), std::string("t4"));
}

TEST("repeat all wraps around")
{
    PlayQueue queue = fourTracks();
    queue.setRepeat(Repeat::All);
    queue.jumpTo(3);
    CHECK(queue.next(false));
    CHECK_EQ(currentId(queue), std::string("t1"));
    CHECK(queue.previous());
    CHECK_EQ(currentId(queue), std::string("t4"));
}

TEST("repeat one holds the track unless the listener skips")
{
    PlayQueue queue = fourTracks();
    queue.setRepeat(Repeat::One);
    CHECK(queue.next(false));
    CHECK_EQ(currentId(queue), std::string("t1"));
    CHECK(queue.next(true));
    CHECK_EQ(currentId(queue), std::string("t2"));
}

TEST("shuffle keeps the current track first and visits everything once")
{
    PlayQueue queue = fourTracks();
    queue.jumpTo(2);
    queue.setShuffle(true, 1234);
    CHECK_EQ(currentId(queue), std::string("t3"));

    std::set<std::string> seen;
    for (const Item *track : queue.upcoming()) {
        seen.insert(track->id);
    }
    CHECK_EQ(seen.size(), static_cast<std::size_t>(4));
    CHECK_EQ(queue.upcoming().front()->id, std::string("t3"));
}

TEST("shuffle is reproducible from its seed")
{
    PlayQueue a = fourTracks();
    PlayQueue b = fourTracks();
    a.setShuffle(true, 42);
    b.setShuffle(true, 42);
    for (int i = 0; i < 3; ++i) {
        a.next(true);
        b.next(true);
        CHECK_EQ(currentId(a), currentId(b));
    }
}

TEST("turning shuffle off restores the original order around the current track")
{
    PlayQueue queue = fourTracks();
    queue.setShuffle(true, 7);
    queue.next(true);
    const std::string playing = currentId(queue);
    queue.setShuffle(false, 0);
    CHECK_EQ(currentId(queue), playing);
    CHECK(!queue.shuffled());
}

TEST("tracks added during shuffle play at the end")
{
    PlayQueue queue = fourTracks();
    queue.setShuffle(true, 99);
    queue.add({4, "t5", "Item 5", "Artist", "Album", 10});
    CHECK_EQ(queue.upcoming().back()->id, std::string("t5"));
}

TEST("removing a track keeps the cursor on the right song")
{
    PlayQueue queue = fourTracks();
    queue.jumpTo(2);
    CHECK(queue.removeAt(0));
    CHECK_EQ(currentId(queue), std::string("t3"));
    CHECK_EQ(queue.size(), static_cast<std::size_t>(3));
    CHECK(queue.removeAt(1));
    CHECK_EQ(currentId(queue), std::string("t4"));
    CHECK(queue.removeAt(1));
    CHECK_EQ(currentId(queue), std::string("t2"));
    CHECK(!queue.removeAt(9));
    CHECK(queue.removeAt(0));
    CHECK(queue.current() == nullptr);
}
