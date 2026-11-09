/**
 * @file    Lifecycle.hpp
 * @brief   When the service may exit (Docs/service-lifecycle.md §5).
 */
#pragma once

namespace IC {

/// The service exits only after the startup grace, with no GUI up and no
/// session running. A session keeps the service resident when the GUI goes.
inline bool shouldExit(bool sessionActive, bool guiUp, bool graceOver)
{
    return graceOver && !guiUp && !sessionActive;
}

} // namespace IC
