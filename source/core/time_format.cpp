#include "time_format.hpp"

#include <cstdio>

namespace core {

std::string formatClock(int seconds)
{
    if (seconds < 0) {
        seconds = 0;
    }
    const int hours = seconds / 3600;
    const int minutes = (seconds / 60) % 60;
    const int secs = seconds % 60;

    char buffer[32];
    if (hours > 0) {
        std::snprintf(buffer, sizeof(buffer), "%d:%02d:%02d", hours, minutes, secs);
    } else {
        std::snprintf(buffer, sizeof(buffer), "%d:%02d", minutes, secs);
    }
    return buffer;
}

float progress(int positionSeconds, int durationSeconds)
{
    if (durationSeconds <= 0 || positionSeconds <= 0) {
        return 0.0f;
    }
    if (positionSeconds >= durationSeconds) {
        return 1.0f;
    }
    return static_cast<float>(positionSeconds) / static_cast<float>(durationSeconds);
}

} // namespace core
