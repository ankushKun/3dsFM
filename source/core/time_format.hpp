#pragma once

#include <string>

namespace core {

// "3:07" for 187 seconds, "1:02:03" once there is an hour. Negative values
// clamp to "0:00" so a bad duration never shows a minus sign.
std::string formatClock(int seconds);

// Whole seconds as a share of a duration, from 0.0 to 1.0. A duration of zero
// or less gives 0.0 so a progress bar never divides by zero.
float progress(int positionSeconds, int durationSeconds);

} // namespace core
