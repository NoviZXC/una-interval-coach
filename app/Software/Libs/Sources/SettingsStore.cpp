/**
 * @file    SettingsStore.cpp
 * @brief   Settings as JSON, following Running's SettingsSerializer pattern.
 */
#include "SettingsStore.hpp"

#include <memory>
#include <new>

#include "SDK/JSON/JsonStreamReader.hpp"
#include "SDK/JSON/JsonStreamWriter.hpp"

#define LOG_MODULE_PRX      "IC.Settings"
#define LOG_MODULE_LEVEL    LOG_LEVEL_INFO
#include "SDK/UnaLogger/Logger.h"

namespace {

/// Reads an unsigned value wider than the field, so 999 clamps instead of
/// wrapping, and stores it only if present.
template <typename T>
void readField(const SDK::JsonStreamReader& r, const char* key, T& field, T maxValue)
{
    uint32_t v = 0;
    if (r.get(key, v)) {
        field = static_cast<T>(v > maxValue ? maxValue : v);
    }
}

} // namespace

SettingsStore::SettingsStore(const SDK::Kernel& kernel, const char* path)
    : mKernel(kernel), mPath(path)
{}

bool SettingsStore::save(const IC::Settings& s)
{
    std::unique_ptr<SDK::Interface::IFile> file = mKernel.fs.file(mPath);
    if (!file || !file->open(true, true)) {
        LOG_ERROR("cannot open %s for writing\n", mPath);
        return false;
    }

    SDK::JsonStreamWriter writer(file.get());
    writer.startMap();
    writer.add("version", static_cast<uint8_t>(1));
    writer.add("reps", s.reps);
    writer.add("rep_distance_m", s.repDistanceM);
    writer.add("hr_target", s.hrTarget);
    writer.add("max_rest_s", s.maxRestS);
    writer.endMap();

    file->flush();
    file->close();
    LOG_INFO("saved reps=%u dist=%u hr=%u rest=%u\n",
             s.reps, s.repDistanceM, s.hrTarget, s.maxRestS);
    return true;
}

bool SettingsStore::load(IC::Settings& s)
{
    std::unique_ptr<SDK::Interface::IFile> file = mKernel.fs.file(mPath);
    if (!file || !file->exist() || !file->open(false, false)) {
        return false;
    }

    const size_t size = file->size();
    if (size == 0 || size > 1024) {
        file->close();
        return false;
    }

    std::unique_ptr<char[]> buffer(new (std::nothrow) char[size]);
    size_t read = 0;
    const bool ok = buffer && file->read(buffer.get(), size, read) && read == size;
    file->close();
    if (!ok) {
        return false;
    }

    SDK::JsonStreamReader reader(buffer.get(), size);
    if (!reader.validate()) {
        LOG_WARNING("%s is not valid JSON, using defaults\n", mPath);
        return false;
    }

    IC::Settings loaded;
    readField(reader, "reps", loaded.reps, static_cast<uint8_t>(255));
    readField(reader, "rep_distance_m", loaded.repDistanceM, static_cast<uint16_t>(65535));
    readField(reader, "hr_target", loaded.hrTarget, static_cast<uint8_t>(255));
    readField(reader, "max_rest_s", loaded.maxRestS, static_cast<uint16_t>(65535));
    loaded.clamp();
    s = loaded;
    return true;
}
