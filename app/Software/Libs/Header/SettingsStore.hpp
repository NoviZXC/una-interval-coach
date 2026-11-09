/**
 * @file    SettingsStore.hpp
 * @brief   Loads and saves IC::Settings as JSON in the app's directory.
 */
#pragma once

#include "SDK/Kernel/Kernel.hpp"

#include "IntervalEngine.hpp"

class SettingsStore {
public:
    SettingsStore(const SDK::Kernel& kernel, const char* path);

    /// Fills @p s from the file. On a missing or invalid file, @p s is left
    /// at its defaults and false is returned. Loaded values are clamped.
    bool load(IC::Settings& s);
    bool save(const IC::Settings& s);

private:
    const SDK::Kernel& mKernel;
    const char*        mPath;
};
