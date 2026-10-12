#include "../check.hpp"
#include "time_format.hpp"

#include <string>

using core::formatClock;
using core::progress;

TEST("formatClock shows minutes and padded seconds")
{
    CHECK_EQ(formatClock(0), std::string("0:00"));
    CHECK_EQ(formatClock(7), std::string("0:07"));
    CHECK_EQ(formatClock(187), std::string("3:07"));
    CHECK_EQ(formatClock(600), std::string("10:00"));
}

TEST("formatClock adds hours once needed")
{
    CHECK_EQ(formatClock(3600), std::string("1:00:00"));
    CHECK_EQ(formatClock(3723), std::string("1:02:03"));
}

TEST("formatClock never shows a negative duration")
{
    CHECK_EQ(formatClock(-5), std::string("0:00"));
}

TEST("progress stays inside the bar")
{
    CHECK_EQ(progress(0, 100), 0.0f);
    CHECK_EQ(progress(50, 100), 0.5f);
    CHECK_EQ(progress(150, 100), 1.0f);
    CHECK_EQ(progress(10, 0), 0.0f);
    CHECK_EQ(progress(-10, 100), 0.0f);
}
