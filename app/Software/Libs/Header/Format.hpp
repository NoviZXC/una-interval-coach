/**
 * @file    Format.hpp
 * @brief   Pure text formatting shared by the GUI and tested on the host.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace IC {

/// "M:SS" for under an hour, "H:MM:SS" otherwise. Returns @p out.
inline char* formatDuration(uint32_t seconds, char* out, std::size_t n)
{
    const unsigned h = seconds / 3600u;
    const unsigned m = (seconds / 60u) % 60u;
    const unsigned s = seconds % 60u;
    if (h > 0) {
        std::snprintf(out, n, "%u:%02u:%02u", h, m, s);
    } else {
        std::snprintf(out, n, "%u:%02u", m, s);
    }
    return out;
}

/// Pace "M:SS", or "--:--" when there is none (0) or it is slower than 59:59.
inline char* formatPace(uint32_t secPerKm, char* out, std::size_t n)
{
    if (secPerKm == 0 || secPerKm >= 3600u) {
        std::snprintf(out, n, "--:--");
        return out;
    }
    return formatDuration(secPerKm, out, n);
}

} // namespace IC
