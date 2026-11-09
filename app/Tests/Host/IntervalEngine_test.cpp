// Host tests for IntervalEngine (QA.md section A). One simulated second per step.
#include <gtest/gtest.h>

#include <random>
#include <vector>

#include "Format.hpp"
#include "IntervalEngine.hpp"
#include "Lifecycle.hpp"

using namespace IC;

namespace {

enum class Gps { FIX, NOFIX, NONE };

Settings qaDefaults()
{
    Settings s;
    s.reps = 3;
    s.repDistanceM = 400;
    s.hrTarget = 120;
    s.maxRestS = 120;
    return s;
}

struct Session {
    IntervalEngine e;
    uint32_t t       = 1000;
    float    trueOdo = 0.0f;   // what a perfect receiver would report
    float    speed   = 4.0f;
    Gps      gps     = Gps::FIX;
    bool     hrOn    = true;
    float    hr      = 150.0f;
    uint8_t  trust   = 2;
    std::vector<Event> events;

    explicit Session(const Settings& s = qaDefaults()) : e(s) {}

    void collect()
    {
        Event buf[IntervalEngine::kMaxEvents];
        const std::size_t n = e.takeEvents(buf, IntervalEngine::kMaxEvents);
        events.insert(events.end(), buf, buf + n);
    }

    void step()
    {
        t += 1000;
        trueOdo += speed;
        if (gps == Gps::FIX) {
            e.onGps(t, true, speed, trueOdo);
        } else if (gps == Gps::NOFIX) {
            e.onGps(t, false, 0.0f, 0.0f);
        }
        if (hrOn) {
            e.onHeartRate(t, hr, trust);
        }
        e.onTick(t);
        collect();
    }

    void steps(int n)
    {
        for (int i = 0; i < n; ++i) {
            step();
        }
    }

    void press(Button b)
    {
        e.onButton(t, b);
        collect();
    }

    /// SETUP → GPS_WAIT → first rep.
    void start()
    {
        for (int i = 0; i < 4; ++i) {
            press(Button::R1);
        }
        step();  // first fix
        press(Button::R1);
        events.clear();
    }

    /// Steps until the state is reached; fails after maxSteps.
    void until(State s, int maxSteps = 1000)
    {
        for (int i = 0; i < maxSteps && e.state() != s; ++i) {
            step();
        }
        ASSERT_EQ(e.state(), s);
    }

    int count(Event ev) const
    {
        int n = 0;
        for (Event x : events) {
            n += (x == ev);
        }
        return n;
    }

    View view() const { return e.view(t); }

    uint32_t restElapsed() const { return e.view(t).restElapsedMs; }
};

} // namespace

// ------------------------------------------------------------- A1 GPS loss

TEST(GpsLoss, ShortLossIsBridged)
{
    Session r;
    r.start();
    r.steps(50);
    EXPECT_FLOAT_EQ(r.view().repDistanceM, 200.0f);
    r.gps = Gps::NOFIX;
    r.steps(6);
    EXPECT_FLOAT_EQ(r.view().repDistanceM, 224.0f);
    EXPECT_EQ(r.count(Event::GPS_LOST), 0);
    EXPECT_FALSE(r.view().gpsLost);
    r.gps = Gps::FIX;
    r.until(State::REST);
    EXPECT_FALSE(r.e.summary().reps[0].gpsGap);
}

TEST(GpsLoss, LongLossFreezesAfter10s)
{
    Session r;
    r.start();
    r.steps(50);
    r.gps = Gps::NOFIX;
    r.steps(9);
    EXPECT_EQ(r.count(Event::GPS_LOST), 0);
    r.step();  // 10 s after the last fix
    EXPECT_EQ(r.count(Event::GPS_LOST), 1);
    r.steps(15);
    EXPECT_FLOAT_EQ(r.view().repDistanceM, 240.0f);
    EXPECT_EQ(r.count(Event::GPS_LOST), 1);
    EXPECT_TRUE(r.view().gpsLost);
}

TEST(GpsLoss, FixReturnResumesWithoutJump)
{
    Session r;
    r.start();
    r.steps(50);
    r.gps = Gps::NOFIX;
    r.steps(25);
    r.gps = Gps::FIX;
    r.step();  // odometer is now 104 m ahead of the last fixed sample
    EXPECT_FLOAT_EQ(r.view().repDistanceM, 240.0f);
    EXPECT_EQ(r.count(Event::GPS_BACK), 1);
    EXPECT_FALSE(r.view().gpsLost);
    r.step();
    EXPECT_FLOAT_EQ(r.view().repDistanceM, 244.0f);
    r.until(State::REST);
    EXPECT_TRUE(r.e.summary().reps[0].gpsGap);
}

TEST(GpsLoss, BridgeCanFinishRep)
{
    Session r;
    r.start();
    r.steps(95);
    EXPECT_FLOAT_EQ(r.view().repDistanceM, 380.0f);
    r.gps = Gps::NOFIX;
    r.steps(4);
    EXPECT_EQ(r.e.state(), State::REP);
    r.step();
    EXPECT_EQ(r.e.state(), State::REST);
    EXPECT_EQ(r.count(Event::REP_DONE), 1);
}

TEST(GpsLoss, LossDuringRestIsIgnored)
{
    Session r;
    r.start();
    r.until(State::REST);
    r.events.clear();
    r.gps = Gps::NOFIX;
    while (r.e.state() == State::REST) {
        r.step();
    }
    ASSERT_EQ(r.count(Event::GO), 1);
    EXPECT_EQ(r.count(Event::GPS_LOST), 0);
    EXPECT_EQ(r.count(Event::GPS_BACK), 0);
    EXPECT_EQ(r.e.summary().rests[0].reason, RestEnd::MAX);
}

TEST(GpsLoss, RepStartsWithoutFix)
{
    Session r;
    r.start();
    r.until(State::REST);
    r.gps = Gps::NOFIX;
    r.until(State::REP);
    r.events.clear();
    EXPECT_FLOAT_EQ(r.view().repDistanceM, 0.0f);
    r.steps(9);
    EXPECT_FLOAT_EQ(r.view().repDistanceM, 0.0f);
    EXPECT_EQ(r.count(Event::GPS_LOST), 0);
    r.step();
    EXPECT_EQ(r.count(Event::GPS_LOST), 1);
}

TEST(GpsLoss, SingleDroppedSamples)
{
    Session r;
    r.start();
    const float odoAtStart = r.trueOdo;
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    int dropped = 0;
    while (r.e.state() == State::REP) {
        const bool drop = u(rng) < 0.02f;
        dropped += drop;
        r.gps = drop ? Gps::NOFIX : Gps::FIX;
        r.step();
    }
    ASSERT_EQ(r.e.state(), State::REST);
    const float truth = r.trueOdo - odoAtStart;
    EXPECT_NEAR(truth, 400.0f, 4.0f);  // within 1 %
    EXPECT_EQ(r.count(Event::GPS_LOST), 0);
    RecordProperty("dropped", dropped);
}

// ------------------------------------------------------------- A2 HR at target

namespace {
/// Finishes rep 1 and leaves the run at rest elapsed 0 (HR 150 so far).
void toRest(Session& r)
{
    r.start();
    r.until(State::REST);
    r.events.clear();
    ASSERT_EQ(r.restElapsed(), 0u);
}

/// Steps until rest has run for @p ms (HR as currently set).
void restTo(Session& r, uint32_t ms)
{
    while (r.e.state() == State::REST && r.restElapsed() < ms) {
        r.step();
    }
}

void feed(Session& r, std::initializer_list<float> bpms)
{
    for (float b : bpms) {
        r.hr = b;
        r.step();
    }
}
} // namespace

TEST(HrAtTarget, EqualNeverEndsRest)
{
    Session r;
    toRest(r);
    r.hr = 120.0f;
    r.until(State::REP);
    EXPECT_EQ(r.e.summary().rests[0].reason, RestEnd::MAX);
    EXPECT_EQ(r.e.summary().rests[0].lengthMs, 120000u);
}

TEST(HrAtTarget, ThreeSecondsBelowEnds)
{
    Session r;
    toRest(r);
    restTo(r, 19000);
    feed(r, {121, 120, 119, 118});
    EXPECT_EQ(r.e.state(), State::REST);
    feed(r, {117});
    EXPECT_EQ(r.e.state(), State::REP);
    EXPECT_EQ(r.e.summary().rests[0].reason, RestEnd::HR);
    EXPECT_EQ(r.e.summary().rests[0].lengthMs, 24000u);
    EXPECT_EQ(r.count(Event::GO), 1);
}

TEST(HrAtTarget, ShortDipDoesNotEnd)
{
    Session r;
    toRest(r);
    while (r.e.state() == State::REST) {
        feed(r, {119, 118, 121});
    }
    EXPECT_EQ(r.e.summary().rests[0].reason, RestEnd::MAX);
}

TEST(HrAtTarget, LowTrustResetsCount)
{
    Session r;
    toRest(r);
    restTo(r, 19000);
    feed(r, {119, 118});
    r.trust = 0;
    feed(r, {110});
    r.trust = 2;
    feed(r, {119, 118});
    EXPECT_EQ(r.e.state(), State::REST);
    feed(r, {117});
    EXPECT_EQ(r.e.state(), State::REP);
    EXPECT_EQ(r.e.summary().rests[0].reason, RestEnd::HR);
}

TEST(HrAtTarget, OneGoOnly)
{
    Session r;
    toRest(r);
    restTo(r, 19000);
    feed(r, {119, 118, 117, 110, 105, 100, 100, 100});
    EXPECT_EQ(r.count(Event::GO), 1);
    EXPECT_EQ(r.e.state(), State::REP);
}

TEST(HrAtTarget, MinimumRest15s)
{
    Session r;
    r.start();
    r.hr = 150.0f;
    r.steps(99);
    r.hr = 100.0f;  // the sample that arrives with the rep end is already low
    r.step();
    ASSERT_EQ(r.e.state(), State::REST);
    r.until(State::REP);
    EXPECT_EQ(r.e.summary().rests[0].reason, RestEnd::HR);
    EXPECT_EQ(r.e.summary().rests[0].lengthMs, 15000u);
}

TEST(HrAtTarget, BelowJustBefore15s)
{
    Session r;
    toRest(r);
    restTo(r, 12000);
    r.hr = 100.0f;
    r.until(State::REP);
    EXPECT_EQ(r.e.summary().rests[0].reason, RestEnd::HR);
    EXPECT_EQ(r.e.summary().rests[0].lengthMs, 15000u);
}

// ------------------------------------------------------------- A3 Max rest

TEST(MaxRest, HighHrEndsAtMax)
{
    Session r;
    toRest(r);
    r.until(State::REP);
    EXPECT_EQ(r.e.summary().rests[0].reason, RestEnd::MAX);
    EXPECT_EQ(r.e.summary().rests[0].lengthMs, 120000u);
    EXPECT_EQ(r.count(Event::GO), 1);
}

TEST(MaxRest, NoHrEndsAtMaxFlagged)
{
    Session r;
    r.start();
    r.steps(99);
    r.hrOn = false;
    r.step();
    ASSERT_EQ(r.e.state(), State::REST);
    r.steps(5);
    EXPECT_EQ(r.view().hr, 0);
    r.until(State::REP);
    EXPECT_EQ(r.e.summary().rests[0].reason, RestEnd::NO_HR);
    EXPECT_EQ(r.e.summary().rests[0].lengthMs, 120000u);
}

TEST(MaxRest, HrLostMidRest)
{
    Session r;
    toRest(r);
    r.hr = 140.0f;
    restTo(r, 30000);
    r.hrOn = false;
    r.until(State::REP);
    EXPECT_EQ(r.e.summary().rests[0].reason, RestEnd::NO_HR);
}

TEST(MaxRest, MaxRestBounds)
{
    for (uint16_t maxS : {uint16_t(30), uint16_t(300)}) {
        Settings s = qaDefaults();
        s.maxRestS = maxS;
        Session r(s);
        toRest(r);
        r.until(State::REP, 400);
        EXPECT_EQ(r.e.summary().rests[0].lengthMs, maxS * 1000u);
    }
}

// ------------------------------------------------------------- A4 Pause

namespace {
void toPausedAt100m(Session& r)
{
    r.speed = 5.0f;
    r.start();
    r.steps(20);
    ASSERT_FLOAT_EQ(r.view().repDistanceM, 100.0f);
    r.press(Button::R1);
}
} // namespace

TEST(Pause, PauseFreezesDistanceAndTime)
{
    Session r;
    toPausedAt100m(r);
    EXPECT_EQ(r.e.state(), State::PAUSED);
    EXPECT_EQ(r.count(Event::PAUSED), 1);
    r.steps(30);
    EXPECT_FLOAT_EQ(r.view().repDistanceM, 100.0f);
    EXPECT_EQ(r.view().repTimeMs, 20000u);
}

TEST(Pause, ResumeContinues)
{
    Session r;
    toPausedAt100m(r);
    r.steps(30);
    r.press(Button::R1);
    EXPECT_EQ(r.e.state(), State::REP);
    EXPECT_EQ(r.count(Event::RESUMED), 1);
    r.step();
    EXPECT_FLOAT_EQ(r.view().repDistanceM, 105.0f);
    EXPECT_EQ(r.view().repTimeMs, 21000u);
}

TEST(Pause, PausedTimeExcludedFromTotals)
{
    Settings s = qaDefaults();
    s.reps = 1;
    Session r(s);
    toPausedAt100m(r);
    r.steps(30);
    r.press(Button::R1);
    r.until(State::SUMMARY);
    EXPECT_EQ(r.e.summary().reps[0].timeMs, 80000u);
    EXPECT_EQ(r.e.summary().totalMs, 80000u);
}

TEST(Pause, NoBridgeWhilePaused)
{
    Session r;
    toPausedAt100m(r);
    r.gps = Gps::NOFIX;
    r.steps(20);
    r.gps = Gps::FIX;
    r.step();
    r.press(Button::R1);
    r.until(State::REST);
    EXPECT_EQ(r.count(Event::GPS_LOST), 0);
    EXPECT_FALSE(r.e.summary().reps[0].gpsGap);
}

TEST(Pause, R1InRestDoesNotPause)
{
    Session r;
    toRest(r);
    r.press(Button::R1);
    EXPECT_EQ(r.count(Event::PAUSED), 0);
    EXPECT_EQ(r.e.summary().rests[0].reason, RestEnd::SKIP);
}

TEST(Pause, EndConfirmFromPause)
{
    Session r;
    toPausedAt100m(r);
    r.press(Button::R2);
    EXPECT_EQ(r.e.state(), State::CONFIRM_END);
    r.press(Button::R2);
    EXPECT_EQ(r.e.state(), State::PAUSED);
    EXPECT_FLOAT_EQ(r.view().repDistanceM, 100.0f);
}

// ------------------------------------------------------------- A5 Last rep

TEST(LastRep, LastRepGoesToSummary)
{
    Settings s = qaDefaults();
    s.reps = 2;
    Session r(s);
    r.start();
    r.until(State::SUMMARY, 1000);
    const std::vector<Event> expected{Event::REP_DONE, Event::GO, Event::SESSION_DONE};
    EXPECT_EQ(r.events, expected);
}

TEST(LastRep, SummaryContents)
{
    Settings s = qaDefaults();
    s.reps = 2;
    Session r(s);
    r.start();
    r.until(State::SUMMARY, 1000);
    const Summary& sum = r.e.summary();
    EXPECT_EQ(sum.repsDone, 2);
    EXPECT_EQ(sum.restsDone, 1);
    EXPECT_EQ(sum.reps[0].timeMs, 100000u);
    EXPECT_EQ(paceSecPerKm(sum.reps[0].timeMs, sum.reps[0].distanceM), 250u);
    EXPECT_EQ(sum.reps[0].avgHr, 150);
    EXPECT_EQ(sum.rests[0].reason, RestEnd::MAX);
    EXPECT_EQ(sum.rests[0].lengthMs, 120000u);
}

TEST(LastRep, SingleRep)
{
    Settings s = qaDefaults();
    s.reps = 1;
    Session r(s);
    r.start();
    r.until(State::SUMMARY);
    EXPECT_EQ(r.events, std::vector<Event>{Event::SESSION_DONE});
    EXPECT_EQ(r.e.summary().restsDone, 0);
}

TEST(LastRep, EarlyEndKeepsCompletedReps)
{
    Settings s = qaDefaults();
    s.reps = 4;
    Session r(s);
    r.start();
    r.until(State::REST);
    r.press(Button::R1);
    r.until(State::REST);
    r.press(Button::R1);
    r.steps(50);
    r.press(Button::R2);
    ASSERT_EQ(r.e.state(), State::CONFIRM_END);
    r.press(Button::R1);
    EXPECT_EQ(r.e.state(), State::SUMMARY);
    EXPECT_EQ(r.e.summary().repsDone, 2);
    EXPECT_EQ(r.count(Event::SESSION_DONE), 0);
}

// ------------------------------------------------------------- A6 Rest

TEST(Rest, SkipEndsRestAsSkip)
{
    Session r;
    toRest(r);
    r.press(Button::R1);
    EXPECT_EQ(r.e.summary().rests[0].reason, RestEnd::SKIP);
    EXPECT_EQ(r.count(Event::GO), 1);
    EXPECT_EQ(r.e.state(), State::REP);
    EXPECT_EQ(r.view().rep, 2);
    EXPECT_FLOAT_EQ(r.view().repDistanceM, 0.0f);
}

// ------------------------------------------------------------- A7 Setup

TEST(Setup, DefaultsWhenNoFile)
{
    const Settings s;
    EXPECT_EQ(s.reps, 6);
    EXPECT_EQ(s.repDistanceM, 400);
    EXPECT_EQ(s.hrTarget, 120);
    EXPECT_EQ(s.maxRestS, 120);
    IntervalEngine e;
    EXPECT_EQ(e.state(), State::SETUP);
    EXPECT_EQ(e.view(0).setupField, 0);
}

TEST(Setup, ClampAtLimits)
{
    IntervalEngine e;
    for (int field = 0; field < 4; ++field) {
        for (int i = 0; i < 100; ++i) {
            e.onButton(0, Button::L1);
        }
        e.onButton(0, Button::R1);
    }
    Settings s = e.settings();
    EXPECT_EQ(s.reps, 20);
    EXPECT_EQ(s.repDistanceM, 2000);
    EXPECT_EQ(s.hrTarget, 160);
    EXPECT_EQ(s.maxRestS, 300);

    IntervalEngine f;
    for (int field = 0; field < 4; ++field) {
        for (int i = 0; i < 100; ++i) {
            f.onButton(0, Button::L2);
        }
        f.onButton(0, Button::R1);
    }
    s = f.settings();
    EXPECT_EQ(s.reps, 1);
    EXPECT_EQ(s.repDistanceM, 100);
    EXPECT_EQ(s.hrTarget, 90);
    EXPECT_EQ(s.maxRestS, 30);
}

TEST(Setup, StepSizes)
{
    IntervalEngine e;
    e.onButton(0, Button::L1); e.onButton(0, Button::R1);
    e.onButton(0, Button::L1); e.onButton(0, Button::R1);
    e.onButton(0, Button::L2); e.onButton(0, Button::R1);
    e.onButton(0, Button::L2);
    const Settings s = e.settings();
    EXPECT_EQ(s.reps, 7);
    EXPECT_EQ(s.repDistanceM, 500);
    EXPECT_EQ(s.hrTarget, 115);
    EXPECT_EQ(s.maxRestS, 105);
}

TEST(Setup, NavigationEvents)
{
    Session r;
    r.press(Button::R2);
    EXPECT_EQ(r.count(Event::EXIT_APP), 1);
    for (int i = 0; i < 4; ++i) {
        r.press(Button::R1);
    }
    EXPECT_EQ(r.e.state(), State::GPS_WAIT);
    EXPECT_EQ(r.count(Event::SETTINGS_COMMITTED), 1);
    r.press(Button::R2);
    EXPECT_EQ(r.e.state(), State::SETUP);
    EXPECT_EQ(r.view().setupField, 3);
}

TEST(Setup, StartNeedsFix)
{
    Session r;
    for (int i = 0; i < 4; ++i) {
        r.press(Button::R1);
    }
    r.press(Button::R1);
    EXPECT_EQ(r.e.state(), State::GPS_WAIT);
    r.gps = Gps::NOFIX;
    r.step();
    r.press(Button::R1);
    EXPECT_EQ(r.e.state(), State::GPS_WAIT);
    r.gps = Gps::FIX;
    r.step();
    r.press(Button::R1);
    EXPECT_EQ(r.e.state(), State::REP);
    EXPECT_EQ(r.view().rep, 1);
}

TEST(Setup, ClampOnLoad)
{
    Settings s;
    s.reps = 99;
    s.repDistanceM = 5;
    s.hrTarget = 250;
    s.maxRestS = 0;
    s.clamp();
    EXPECT_EQ(s.reps, 20);
    EXPECT_EQ(s.repDistanceM, 100);
    EXPECT_EQ(s.hrTarget, 160);
    EXPECT_EQ(s.maxRestS, 30);
}

// ------------------------------------------------------------- A9 Lifecycle

TEST(Lifecycle, StaysWhileSessionActive) { EXPECT_FALSE(shouldExit(true, false, true)); }
TEST(Lifecycle, ExitsWhenIdleAndNoGui)   { EXPECT_TRUE(shouldExit(false, false, true)); }
TEST(Lifecycle, NeverExitsBeforeGrace)   { EXPECT_FALSE(shouldExit(false, false, false)); }
TEST(Lifecycle, StaysWhileGuiUp)         { EXPECT_FALSE(shouldExit(false, true, true)); }

TEST(Lifecycle, EngineAdvancesWithoutGui)
{
    // The engine has no notion of a GUI: sensor input and ticks alone run a
    // whole session, so nothing depends on GUI frames (service-lifecycle §6).
    Settings s = qaDefaults();
    s.reps = 2;
    Session r(s);
    r.start();
    r.until(State::SUMMARY, 1000);
    EXPECT_EQ(r.e.summary().repsDone, 2);
}

// ------------------------------------------------------------- A10 Summary

TEST(Summary, PaceFormat)
{
    char buf[12];
    EXPECT_EQ(paceSecPerKm(96000, 400.0f), 240u);
    EXPECT_STREQ(formatPace(240, buf, sizeof buf), "4:00");
    EXPECT_STREQ(formatPace(0, buf, sizeof buf), "--:--");
    EXPECT_STREQ(formatDuration(75, buf, sizeof buf), "1:15");
    EXPECT_STREQ(formatDuration(3725, buf, sizeof buf), "1:02:05");
}

TEST(Summary, AvgHrIgnoresInvalid)
{
    Settings s = qaDefaults();
    s.reps = 1;
    Session r(s);
    r.start();
    bool good = true;
    while (r.e.state() == State::REP) {
        r.hr    = good ? 150.0f : 90.0f;
        r.trust = good ? 2 : 0;
        good = !good;
        r.step();
    }
    EXPECT_EQ(r.e.summary().reps[0].avgHr, 150);
}

TEST(Summary, TotalTimeIncludesRests)
{
    Settings s = qaDefaults();
    s.reps = 2;
    Session r(s);
    r.start();
    r.until(State::REST);
    r.steps(10);
    r.press(Button::R1);
    r.until(State::SUMMARY);
    EXPECT_EQ(r.e.summary().totalMs, 210000u);
}

TEST(Summary, ScrollStaysInRange)
{
    Settings s = qaDefaults();
    s.reps = 2;
    Session r(s);
    r.start();
    r.until(State::SUMMARY, 1000);
    r.press(Button::L1);
    EXPECT_EQ(r.view().summaryScroll, 0);
    r.press(Button::L2);
    r.press(Button::L2);
    EXPECT_EQ(r.view().summaryScroll, 1);
    r.press(Button::R2);
    EXPECT_EQ(r.count(Event::EXIT_APP), 1);
}
