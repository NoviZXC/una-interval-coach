/**
 * @file    IntervalEngine.cpp
 * @brief   Interval Coach session rules. See IntervalEngine.hpp and SPEC.md.
 */
#include "IntervalEngine.hpp"

#include <cmath>

namespace IC {

namespace {

template <typename T>
T clampTo(T v, T lo, T hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

template <typename T>
T stepUp(T v, T step, T hi)
{
    return (v + step > hi) ? v : static_cast<T>(v + step);
}

template <typename T>
T stepDown(T v, T step, T lo)
{
    return (v < lo + step) ? v : static_cast<T>(v - step);
}

} // namespace

// ---------------------------------------------------------------- Settings

void Settings::clamp()
{
    reps         = clampTo<uint8_t>(reps, kRepsMin, kRepsMax);
    repDistanceM = clampTo<uint16_t>(repDistanceM, kDistMin, kDistMax);
    hrTarget     = clampTo<uint8_t>(hrTarget, kHrMin, kHrMax);
    maxRestS     = clampTo<uint16_t>(maxRestS, kRestMin, kRestMax);
}

// ---------------------------------------------------------------- Engine

IntervalEngine::IntervalEngine(const Settings& settings)
    : mSettings(settings)
{
    mSettings.clamp();
}

bool IntervalEngine::isValidHr(float bpm, uint8_t trust)
{
    return trust >= 1 && trust <= 3 && bpm > 20.0f;
}

State IntervalEngine::state() const
{
    return mConfirm ? State::CONFIRM_END : mState;
}

bool IntervalEngine::sessionActive() const
{
    return mState == State::REP || mState == State::PAUSED || mState == State::REST;
}

std::size_t IntervalEngine::takeEvents(Event* out, std::size_t max)
{
    std::size_t n = mEventCount < max ? mEventCount : max;
    for (std::size_t i = 0; i < n; ++i) {
        out[i] = mEvents[i];
    }
    mEventCount = 0;
    return n;
}

void IntervalEngine::push(Event e)
{
    if (mEventCount < mEvents.size()) {
        mEvents[mEventCount++] = e;
    }
}

// ---------------------------------------------------------------- Inputs

void IntervalEngine::onTick(uint32_t nowMs)
{
    if (mState == State::REP) {
        updateGap(nowMs);
        bridgeTo(nowMs);
        checkRepDone(nowMs);
    } else if (mState == State::REST) {
        checkRest(nowMs);
    }
}

void IntervalEngine::onGps(uint32_t nowMs, bool fix, float speedMps, float odometerM)
{
    if (!fix) {
        mFix = false;
        if (mState == State::REP) {
            updateGap(nowMs);
            bridgeTo(nowMs);
            checkRepDone(nowMs);
        }
        return;
    }

    if (mState == State::REP) {
        if (mInGap) {
            // Bridge up to now, then re-baseline: the odometer jump that
            // covers the lost stretch is not added.
            bridgeTo(nowMs);
            mInGap = false;
            if (mGpsLost) {
                mGpsLost = false;
                push(Event::GPS_BACK);
            }
        } else if (mHaveOdo && odometerM > mLastOdo) {
            mRepDistance += odometerM - mLastOdo;
        }
    }

    mFix       = true;
    mHaveOdo   = true;
    mLastOdo   = odometerM;
    mLastSpeed = speedMps > 0.0f ? speedMps : 0.0f;
    mLastFixMs = nowMs;

    if (mState == State::REP) {
        checkRepDone(nowMs);
    }
}

void IntervalEngine::onHeartRate(uint32_t nowMs, float bpm, uint8_t trust)
{
    if (!isValidHr(bpm, trust)) {
        mBelowRun = false;
        return;
    }

    mHr            = bpm;
    mLastValidHrMs = nowMs;
    mHaveValidHr   = true;

    if (mState == State::REP) {
        mRepHrSum += bpm;
        ++mRepHrCount;
    } else if (mState == State::REST) {
        if (bpm < mSettings.hrTarget) {
            if (!mBelowRun) {
                mBelowRun     = true;
                mBelowSinceMs = nowMs;
            }
        } else {
            mBelowRun = false;
        }
        checkRest(nowMs);
    }
}

void IntervalEngine::onButton(uint32_t nowMs, Button b)
{
    if (mConfirm) {
        if (b == Button::R1) {
            endSession(nowMs);
        } else if (b == Button::R2) {
            mConfirm = false;
        }
        return;
    }

    switch (mState) {
    case State::SETUP:
        handleSetupButton(b);
        break;

    case State::GPS_WAIT:
        if (b == Button::R1 && mFix) {
            startSession(nowMs);
        } else if (b == Button::R2) {
            mState = State::SETUP;
        }
        break;

    case State::REP:
        if (b == Button::R1) {
            pause(nowMs);
        } else if (b == Button::R2) {
            mConfirm = true;
        }
        break;

    case State::PAUSED:
        if (b == Button::R1) {
            resume(nowMs);
        } else if (b == Button::R2) {
            mConfirm = true;
        }
        break;

    case State::REST:
        if (b == Button::R1) {
            endRest(nowMs, RestEnd::SKIP);
        } else if (b == Button::R2) {
            mConfirm = true;
        }
        break;

    case State::SUMMARY:
        if (b == Button::L1 && mScroll > 0) {
            --mScroll;
        } else if (b == Button::L2 && mScroll + 1 < mSummary.repsDone) {
            ++mScroll;
        } else if (b == Button::R1 || b == Button::R2) {
            push(Event::EXIT_APP);
        }
        break;

    case State::CONFIRM_END:
        break;  // Never stored in mState; mConfirm handles it above.
    }
}

void IntervalEngine::handleSetupButton(Button b)
{
    Settings& s = mSettings;
    const bool up = (b == Button::L1);

    if (b == Button::L1 || b == Button::L2) {
        switch (mSetupField) {
        case 0: s.reps = up ? stepUp(s.reps, Settings::kRepsStep, Settings::kRepsMax)
                            : stepDown(s.reps, Settings::kRepsStep, Settings::kRepsMin); break;
        case 1: s.repDistanceM = up ? stepUp(s.repDistanceM, Settings::kDistStep, Settings::kDistMax)
                                    : stepDown(s.repDistanceM, Settings::kDistStep, Settings::kDistMin); break;
        case 2: s.hrTarget = up ? stepUp(s.hrTarget, Settings::kHrStep, Settings::kHrMax)
                                : stepDown(s.hrTarget, Settings::kHrStep, Settings::kHrMin); break;
        default: s.maxRestS = up ? stepUp(s.maxRestS, Settings::kRestStep, Settings::kRestMax)
                                 : stepDown(s.maxRestS, Settings::kRestStep, Settings::kRestMin); break;
        }
    } else if (b == Button::R1) {
        if (mSetupField < 3) {
            ++mSetupField;
        } else {
            mState = State::GPS_WAIT;
            push(Event::SETTINGS_COMMITTED);
        }
    } else if (b == Button::R2) {
        if (mSetupField > 0) {
            --mSetupField;
        } else {
            push(Event::EXIT_APP);
        }
    }
}

// ---------------------------------------------------------------- Session

void IntervalEngine::startSession(uint32_t nowMs)
{
    mSummary         = Summary{};
    mSummary.planned = mSettings.reps;
    mSessionStartMs  = nowMs;
    mSessionPausedMs = 0;
    mRep             = 1;
    startRep(nowMs);
}

void IntervalEngine::startRep(uint32_t nowMs)
{
    mState       = State::REP;
    mConfirm     = false;
    mRepDistance = 0.0f;
    mRepStartMs  = nowMs;
    mRepPausedMs = 0;
    mRepHrSum    = 0.0f;
    mRepHrCount  = 0;
    mRepGap      = false;
    mGpsLost     = false;

    if (mFix && nowMs - mLastFixMs <= kGpsTimeoutMs) {
        mInGap = false;
    } else {
        // No fix at the start: nothing to bridge with.
        mInGap       = true;
        mGapStartMs  = nowMs;
        mBridgedToMs = nowMs;
        mLastSpeed   = 0.0f;
    }
}

void IntervalEngine::finishRep(uint32_t nowMs)
{
    RepResult r;
    r.timeMs    = nowMs - mRepStartMs - mRepPausedMs;
    r.distanceM = mRepDistance;
    r.avgHr     = mRepHrCount ? static_cast<uint8_t>(std::lround(mRepHrSum / mRepHrCount)) : 0;
    r.gpsGap    = mRepGap;
    mSummary.reps[mSummary.repsDone++] = r;
    mInGap   = false;
    mGpsLost = false;

    if (mRep >= mSettings.reps) {
        endSession(nowMs);
        push(Event::SESSION_DONE);
        return;
    }

    mState       = State::REST;
    mConfirm     = false;
    mRestStartMs = nowMs;
    mBelowRun    = false;
    push(Event::REP_DONE);
}

void IntervalEngine::endRest(uint32_t nowMs, RestEnd reason)
{
    RestResult r;
    r.lengthMs = nowMs - mRestStartMs;
    r.reason   = reason;
    mSummary.rests[mSummary.restsDone++] = r;
    ++mRep;
    startRep(nowMs);
    push(Event::GO);
}

void IntervalEngine::endSession(uint32_t nowMs)
{
    if (mState == State::PAUSED) {
        mSessionPausedMs += nowMs - mPauseStartMs;
    }
    mSummary.totalMs = nowMs - mSessionStartMs - mSessionPausedMs;
    mState   = State::SUMMARY;
    mConfirm = false;
    mScroll  = 0;
    mInGap   = false;
    mGpsLost = false;
}

void IntervalEngine::pause(uint32_t nowMs)
{
    bridgeTo(nowMs);
    mInGap        = false;
    mState        = State::PAUSED;
    mPauseStartMs = nowMs;
    push(Event::PAUSED);
}

void IntervalEngine::resume(uint32_t nowMs)
{
    const uint32_t paused = nowMs - mPauseStartMs;
    mRepPausedMs     += paused;
    mSessionPausedMs += paused;
    mState = State::REP;
    push(Event::RESUMED);

    if (mFix && nowMs - mLastFixMs <= kGpsTimeoutMs) {
        mInGap = false;
        if (mGpsLost) {
            mGpsLost = false;
            push(Event::GPS_BACK);
        }
    } else {
        mInGap       = true;
        mGapStartMs  = nowMs;
        mBridgedToMs = nowMs;
    }
}

// ---------------------------------------------------------------- GPS gap

void IntervalEngine::updateGap(uint32_t nowMs)
{
    if (mInGap) {
        return;
    }
    if (!mFix || nowMs - mLastFixMs > kGpsTimeoutMs) {
        mInGap       = true;
        mGapStartMs  = mLastFixMs;
        mBridgedToMs = mLastFixMs;
    }
}

void IntervalEngine::bridgeTo(uint32_t nowMs)
{
    if (!mInGap) {
        return;
    }
    const uint32_t elapsed = nowMs - mGapStartMs;
    const uint32_t target  = elapsed < kBridgeMs ? elapsed : kBridgeMs;
    const uint32_t done    = mBridgedToMs - mGapStartMs;
    if (target > done) {
        mRepDistance += mLastSpeed * static_cast<float>(target - done) / 1000.0f;
        mBridgedToMs  = mGapStartMs + target;
    }
    if (elapsed >= kBridgeMs && !mGpsLost) {
        mGpsLost = true;
        mRepGap  = true;
        push(Event::GPS_LOST);
    }
}

void IntervalEngine::checkRepDone(uint32_t nowMs)
{
    if (mState == State::REP && mRepDistance >= mSettings.repDistanceM) {
        finishRep(nowMs);
    }
}

// ---------------------------------------------------------------- Rest

bool IntervalEngine::hrAvailable(uint32_t nowMs) const
{
    return mHaveValidHr && nowMs - mLastValidHrMs <= kHrTimeoutMs;
}

void IntervalEngine::checkRest(uint32_t nowMs)
{
    if (mState != State::REST) {
        return;
    }
    const uint32_t elapsed = nowMs - mRestStartMs;

    if (mBelowRun && nowMs - mLastValidHrMs > kHrTimeoutMs) {
        mBelowRun = false;
    }
    if (mBelowRun && mLastValidHrMs - mBelowSinceMs >= kHrBelowSpanMs && elapsed >= kMinRestMs) {
        endRest(nowMs, RestEnd::HR);
        return;
    }
    if (elapsed >= static_cast<uint32_t>(mSettings.maxRestS) * 1000u) {
        endRest(nowMs, hrAvailable(nowMs) ? RestEnd::MAX : RestEnd::NO_HR);
    }
}

// ---------------------------------------------------------------- View

View IntervalEngine::view(uint32_t nowMs) const
{
    View v;
    v.state        = state();
    v.underlying   = mState;
    v.settings     = mSettings;
    v.setupField   = mSetupField;
    v.gpsFix       = mFix && nowMs - mLastFixMs <= kGpsTimeoutMs;
    v.gpsLost      = mGpsLost && (mState == State::REP || mState == State::PAUSED);
    v.rep          = mRep;
    v.repDistanceM = mRepDistance;
    const float left = static_cast<float>(mSettings.repDistanceM) - mRepDistance;
    v.metersLeft   = left > 0.0f ? static_cast<uint32_t>(std::ceil(left)) : 0;
    if (mState == State::REP) {
        v.repTimeMs = nowMs - mRepStartMs - mRepPausedMs;
    } else if (mState == State::PAUSED) {
        v.repTimeMs = mPauseStartMs - mRepStartMs - mRepPausedMs;
    }
    if (v.gpsFix && mLastSpeed > 0.5f) {
        v.paceSecPerKm = static_cast<uint32_t>(std::lround(1000.0f / mLastSpeed));
    }
    v.hr = hrAvailable(nowMs) ? static_cast<uint8_t>(std::lround(mHr)) : 0;
    if (mState == State::REST) {
        const uint32_t maxMs = static_cast<uint32_t>(mSettings.maxRestS) * 1000u;
        v.restElapsedMs   = nowMs - mRestStartMs;
        v.restRemainingMs = v.restElapsedMs < maxMs ? maxMs - v.restElapsedMs : 0;
    }
    v.summaryScroll = mScroll;
    return v;
}

} // namespace IC
