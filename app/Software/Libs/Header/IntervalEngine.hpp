/**
 * @file    IntervalEngine.hpp
 * @brief   Interval Coach session rules (SPEC.md). Pure C++17: no SDK, kernel
 *          or TouchGFX dependencies, so the host tests drive it directly.
 *
 * Time is always passed in (milliseconds, any monotonic origin). The engine
 * never reads a clock and never sleeps.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace IC {

struct Settings {
    static constexpr uint8_t  kRepsMin = 1,    kRepsMax = 20,    kRepsStep = 1;
    static constexpr uint16_t kDistMin = 100,  kDistMax = 2000,  kDistStep = 100;   // m
    static constexpr uint8_t  kHrMin   = 90,   kHrMax   = 160,   kHrStep   = 5;     // bpm
    static constexpr uint16_t kRestMin = 30,   kRestMax = 300,   kRestStep = 15;    // s

    uint8_t  reps         = 6;
    uint16_t repDistanceM = 400;
    uint8_t  hrTarget     = 120;
    uint16_t maxRestS     = 120;

    /// Clamp every field into its range (used after loading from a file).
    void clamp();
};

enum class Button : uint8_t { L1, L2, R1, R2 };

enum class State : uint8_t { SETUP, GPS_WAIT, REP, PAUSED, REST, CONFIRM_END, SUMMARY };

enum class Event : uint8_t {
    REP_DONE,           ///< Rep distance reached, rest starts.
    GO,                 ///< Rest over, next rep started.
    SESSION_DONE,       ///< Last rep finished, summary shown.
    GPS_LOST,           ///< GPS lost for more than 10 s during a rep.
    GPS_BACK,           ///< Fix back after GPS_LOST.
    PAUSED,
    RESUMED,
    SETTINGS_COMMITTED, ///< Setup left forward: save the settings.
    EXIT_APP,           ///< The GUI should exit.
};

enum class RestEnd : uint8_t { HR, MAX, SKIP, NO_HR };

struct RepResult {
    uint32_t timeMs    = 0;
    float    distanceM = 0.0f;
    uint8_t  avgHr     = 0;      ///< 0 = no valid HR during the rep.
    bool     gpsGap    = false;  ///< A GPS loss longer than 10 s happened.
};

struct RestResult {
    uint32_t lengthMs = 0;
    RestEnd  reason   = RestEnd::MAX;
};

struct Summary {
    uint8_t  planned   = 0;
    uint8_t  repsDone  = 0;
    uint32_t totalMs   = 0;      ///< Session time, paused time excluded.
    std::array<RepResult, Settings::kRepsMax>  reps{};
    std::array<RestResult, Settings::kRepsMax> rests{};
    uint8_t  restsDone = 0;
};

/// Average pace in seconds per km; 0 when the distance is under 1 m.
/// Inline: the GUI uses it and does not link IntervalEngine.cpp.
inline uint32_t paceSecPerKm(uint32_t timeMs, float distanceM)
{
    if (distanceM < 1.0f) {
        return 0;
    }
    return static_cast<uint32_t>(timeMs / distanceM + 0.5f);   // ms per m == s per km
}

/// Everything a screen needs to draw.
struct View {
    State    state        = State::SETUP;
    State    underlying   = State::SETUP;  ///< State behind CONFIRM_END.
    Settings settings{};
    uint8_t  setupField   = 0;     ///< 0 reps, 1 distance, 2 HR target, 3 max rest.
    bool     gpsFix       = false;
    bool     gpsLost      = false; ///< "GPS LOST" banner.
    uint8_t  rep          = 0;     ///< 1-based current (or next, in rest) rep.
    float    repDistanceM = 0.0f;
    uint32_t metersLeft   = 0;
    uint32_t repTimeMs    = 0;
    uint32_t paceSecPerKm = 0;     ///< 0 = no pace (no fix or standing still).
    uint8_t  hr           = 0;     ///< 0 = no valid HR ("HR --").
    uint32_t restElapsedMs   = 0;
    uint32_t restRemainingMs = 0;  ///< Time to max rest.
    uint8_t  summaryScroll   = 0;  ///< First summary row shown.
};

class IntervalEngine {
public:
    static constexpr uint32_t kGpsTimeoutMs   = 2000;   ///< No sample for this long = lost.
    static constexpr uint32_t kBridgeMs       = 10000;  ///< Distance bridged for this long.
    static constexpr uint32_t kHrTimeoutMs    = 2000;   ///< No valid HR for this long = none.
    static constexpr uint32_t kHrBelowSpanMs  = 2000;   ///< 3 consecutive 1 Hz samples.
    static constexpr uint32_t kMinRestMs      = 15000;
    static constexpr std::size_t kMaxEvents   = 16;

    explicit IntervalEngine(const Settings& settings = Settings{});

    /// Replaces the settings (clamped). Only meant for SETUP, e.g. after load.
    void setSettings(const Settings& settings)
    {
        mSettings = settings;
        mSettings.clamp();
    }

    // Inputs
    void onTick(uint32_t nowMs);
    void onGps(uint32_t nowMs, bool fix, float speedMps, float odometerM);
    void onHeartRate(uint32_t nowMs, float bpm, uint8_t trust);
    void onButton(uint32_t nowMs, Button button);

    // Outputs
    State          state() const;
    View           view(uint32_t nowMs) const;
    const Summary& summary() const { return mSummary; }
    const Settings& settings() const { return mSettings; }
    bool           sessionActive() const;
    /// Copies pending events into @p out (up to @p max) and clears them.
    std::size_t    takeEvents(Event* out, std::size_t max);

    static bool isValidHr(float bpm, uint8_t trust);

private:
    void handleSetupButton(Button b);
    void startSession(uint32_t nowMs);
    void startRep(uint32_t nowMs);
    void finishRep(uint32_t nowMs);
    void endRest(uint32_t nowMs, RestEnd reason);
    void endSession(uint32_t nowMs);
    void pause(uint32_t nowMs);
    void resume(uint32_t nowMs);
    void updateGap(uint32_t nowMs);
    void bridgeTo(uint32_t nowMs);
    void checkRest(uint32_t nowMs);
    void checkRepDone(uint32_t nowMs);
    bool hrAvailable(uint32_t nowMs) const;
    void push(Event e);

    Settings mSettings;
    State    mState      = State::SETUP;
    bool     mConfirm    = false;
    uint8_t  mSetupField = 0;
    uint8_t  mScroll     = 0;

    // GPS (tracked in every state; distance accrues only in REP)
    bool     mFix          = false;
    bool     mHaveOdo      = false;
    float    mLastOdo      = 0.0f;
    float    mLastSpeed    = 0.0f;   ///< Last speed with a fix.
    uint32_t mLastFixMs    = 0;
    bool     mInGap        = false;
    uint32_t mGapStartMs   = 0;
    uint32_t mBridgedToMs  = 0;
    bool     mGpsLost      = false;

    // Heart rate
    float    mHr           = 0.0f;
    uint32_t mLastValidHrMs = 0;
    bool     mHaveValidHr  = false;
    bool     mBelowRun     = false;
    uint32_t mBelowSinceMs = 0;

    // Session
    uint8_t  mRep          = 0;       ///< 1-based.
    float    mRepDistance  = 0.0f;
    uint32_t mRepStartMs   = 0;
    uint32_t mRepPausedMs  = 0;       ///< Paused time inside this rep.
    uint32_t mPauseStartMs = 0;
    float    mRepHrSum     = 0.0f;
    uint32_t mRepHrCount   = 0;
    bool     mRepGap       = false;
    uint32_t mRestStartMs  = 0;
    uint32_t mSessionStartMs  = 0;
    uint32_t mSessionPausedMs = 0;

    Summary  mSummary{};
    std::array<Event, kMaxEvents> mEvents{};
    std::size_t mEventCount = 0;
};

} // namespace IC
