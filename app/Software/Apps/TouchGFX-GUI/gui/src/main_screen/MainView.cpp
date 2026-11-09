#include <gui/main_screen/MainView.hpp>

#include <cstdio>

#include <touchgfx/Color.hpp>

#include "SDK/GUI/Button.hpp"
#include "SDK/GUI/Color.hpp"

#include "Format.hpp"

namespace {

namespace Col = SDK::GUI::Color;
constexpr uint32_t kAmber = 0xC08000;   // UI kit "primary" (Buttons widget)

// Rows of the layout (top y of each text box). Lower rows sit where the round
// display is narrower, so their text is kept short (checked by check_round.py).
constexpr int16_t kYTitle = 24;
constexpr int16_t kYName  = 60;
constexpr int16_t kYBigHigh = 46;   // Rep / Rest
constexpr int16_t kYBigLow  = 94;   // Setup
constexpr int16_t kYUnit  = 114;
constexpr int16_t kYMid   = 132;
constexpr int16_t kYSmall = 168;

touchgfx::colortype rgb(uint32_t c)
{
    return touchgfx::Color::getColorFromRGB((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
}

const char* restReason(IC::RestEnd r)
{
    switch (r) {
    case IC::RestEnd::HR:    return "HR";
    case IC::RestEnd::MAX:   return "max";
    case IC::RestEnd::SKIP:  return "skip";
    case IC::RestEnd::NO_HR: return "no HR";
    }
    return "";
}

} // namespace

// ------------------------------------------------------------------ Label

void MainView::Label::init(TypedTextId id, int16_t y, int16_t h, int16_t x, int16_t w)
{
    text.setTypedText(touchgfx::TypedText(id));
    text.setWildcard(buf);
    text.setPosition(x, y, w, h);
    text.setVisible(false);
}

void MainView::Label::show(const char* s, uint32_t color, int16_t y)
{
    touchgfx::Unicode::strncpy(buf, s, kLen - 1);
    buf[kLen - 1] = 0;
    text.setColor(rgb(color));
    text.setY(y);
    text.setVisible(true);
}

void MainView::Label::hide()
{
    text.setVisible(false);
}

// ------------------------------------------------------------------ View

MainView::MainView()
{
}

void MainView::setupScreen()
{
    MainViewBase::setupScreen();

    mTitle.init(T_TMP_SEMIBOLD_20, kYTitle, 30);
    mName.init(T_TMP_MEDIUM_25, kYName, 36);
    mBig.init(T_TMP_SEMIBOLD_60, kYBigHigh, 80);
    mUnit.init(T_TMP_REGULAR_18, kYUnit, 26);
    mMid.init(T_TMP_SEMIBOLD_30, kYMid, 42);
    mSmall.init(T_TMP_REGULAR_18, kYSmall, 26);
    for (int i = 0; i < 4; ++i) {
        mRows[i].init(T_TMP_REGULAR_18, 0, 26);
    }
    mFoot.init(T_TMP_REGULAR_18, kYSmall, 26);
    // Pace | HR, split at the centre so the line stays inside the circle.
    mLeft.init(T_TMP_REGULAR_18_R, kYSmall, 26, 0, 114);
    mRight.init(T_TMP_REGULAR_18_L, kYSmall, 26, 124, 116);

    for (Label* l : {&mTitle, &mName, &mBig, &mUnit, &mMid, &mSmall, &mRows[0], &mRows[1], &mRows[2], &mRows[3],
                     &mFoot, &mLeft, &mRight}) {
        add(l->text);
    }

    // The hint arcs are drawn last, on top of everything.
    remove(buttons);
    add(buttons);

    render(mView);
}

void MainView::tearDownScreen()
{
    MainViewBase::tearDownScreen();
}

void MainView::handleKeyEvent(uint8_t key)
{
    switch (key) {
    case SDK::GUI::Button::L1: presenter->button(IC::Button::L1); break;
    case SDK::GUI::Button::L2: presenter->button(IC::Button::L2); break;
    case SDK::GUI::Button::R1: presenter->button(IC::Button::R1); break;
    case SDK::GUI::Button::R2: presenter->button(IC::Button::R2); break;
    default: break;
    }
}

void MainView::hideAll()
{
    mTitle.hide();
    mName.hide();
    mBig.hide();
    mUnit.hide();
    mMid.hide();
    mSmall.hide();
    for (Label& r : mRows) {
        r.hide();
    }
    mFoot.hide();
    mLeft.hide();
    mRight.hide();
}

void MainView::render(const IC::View& v)
{
    mView = v;
    hideAll();

    switch (v.state) {
    case IC::State::SETUP:       renderSetup(v);   break;
    case IC::State::GPS_WAIT:    renderGpsWait(v); break;
    case IC::State::REP:
    case IC::State::PAUSED:      renderRep(v);     break;
    case IC::State::REST:        renderRest(v);    break;
    case IC::State::CONFIRM_END: renderConfirm(v); break;
    case IC::State::SUMMARY:     renderSummaryRows(); break;
    }
    invalidate();
}

void MainView::renderSummary(const IC::Summary& s)
{
    mSummary = s;
    if (mView.state == IC::State::SUMMARY) {
        render(mView);
    }
}

// ------------------------------------------------------------------ Screens

void MainView::renderSetup(const IC::View& v)
{
    static const char* const kNames[4] = {"REPS", "REP DISTANCE", "HR TARGET", "MAX REST"};
    static const char* const kUnits[4] = {"reps", "metres", "bpm", "min:sec"};
    char buf[24];

    std::snprintf(buf, sizeof buf, "SETUP %u/4", v.setupField + 1u);
    mTitle.show(buf, kAmber, kYTitle);
    mName.show(kNames[v.setupField], Col::WHITE, kYName);

    switch (v.setupField) {
    case 0:  std::snprintf(buf, sizeof buf, "%u", v.settings.reps); break;
    case 1:  std::snprintf(buf, sizeof buf, "%u", v.settings.repDistanceM); break;
    case 2:  std::snprintf(buf, sizeof buf, "%u", v.settings.hrTarget); break;
    default: IC::formatDuration(v.settings.maxRestS, buf, sizeof buf); break;
    }
    mBig.show(buf, Col::WHITE, kYBigLow);
    mUnit.show(kUnits[v.setupField], Col::GRAY, kYUnit + 42);

    buttons.setL1(Buttons::WHITE);
    buttons.setL2(Buttons::WHITE);
    buttons.setR1(Buttons::AMBER);
    buttons.setR2(Buttons::WHITE);
}

void MainView::renderGpsWait(const IC::View& v)
{
    char buf[32];
    mTitle.show("GPS", kAmber, kYTitle);
    mMid.show(v.gpsFix ? "GPS ready" : "Searching...", Col::WHITE, 92);
    mUnit.show(v.gpsFix ? "Start: top right" : "Waiting for a fix",
               v.gpsFix ? kAmber : Col::GRAY, 138);
    std::snprintf(buf, sizeof buf, "%u x %u m", v.settings.reps, v.settings.repDistanceM);
    mSmall.show(buf, Col::GRAY, 170);

    buttons.setL1(Buttons::NONE);
    buttons.setL2(Buttons::NONE);
    buttons.setR1(v.gpsFix ? Buttons::AMBER : Buttons::NONE);
    buttons.setR2(Buttons::WHITE);
}

void MainView::renderRep(const IC::View& v)
{
    char buf[32];
    char t[12];
    char p[12];
    const bool paused = (v.state == IC::State::PAUSED);

    if (paused) {
        mTitle.show("PAUSED", kAmber, kYTitle);
    } else if (v.gpsLost) {
        mTitle.show("GPS LOST", Col::RED, kYTitle);
    } else {
        std::snprintf(buf, sizeof buf, "REP %u/%u", v.rep, v.settings.reps);
        mTitle.show(buf, Col::WHITE, kYTitle);
    }

    std::snprintf(buf, sizeof buf, "%lu", static_cast<unsigned long>(v.metersLeft));
    mBig.show(buf, paused ? Col::GRAY : Col::WHITE, kYBigHigh);
    mUnit.show("m to go", Col::GRAY, kYUnit);

    mMid.show(IC::formatDuration(v.repTimeMs / 1000u, t, sizeof t), Col::WHITE, kYMid);
    IC::formatPace(v.paceSecPerKm, p, sizeof p);
    std::snprintf(buf, sizeof buf, "%s/km", p);
    mLeft.show(buf, Col::WHITE, kYSmall);
    if (v.hr) {
        std::snprintf(buf, sizeof buf, "HR %u", v.hr);
    } else {
        std::snprintf(buf, sizeof buf, "HR --");
    }
    mRight.show(buf, Col::WHITE, kYSmall);

    buttons.setL1(Buttons::NONE);
    buttons.setL2(Buttons::NONE);
    buttons.setR1(paused ? Buttons::AMBER : Buttons::WHITE);
    buttons.setR2(Buttons::RED);
}

void MainView::renderRest(const IC::View& v)
{
    char buf[32];
    char t[12];

    mTitle.show("REST", kAmber, kYTitle);   // short: the top of the circle is narrow

    if (v.hr) {
        std::snprintf(buf, sizeof buf, "%u", v.hr);
        mBig.show(buf, v.hr < v.settings.hrTarget ? kAmber : Col::WHITE, kYBigHigh);
    } else {
        mBig.show("--", Col::GRAY, kYBigHigh);
    }
    std::snprintf(buf, sizeof buf, v.hr ? "bpm, target %u" : "HR --, target %u", v.settings.hrTarget);
    mUnit.show(buf, Col::GRAY, kYUnit);

    IC::formatDuration((v.restRemainingMs + 999u) / 1000u, t, sizeof t);
    std::snprintf(buf, sizeof buf, "%s left", t);
    mMid.show(buf, Col::WHITE, kYMid);
    std::snprintf(buf, sizeof buf, "next rep %u/%u", v.rep + 1u, v.settings.reps);
    mSmall.show(buf, Col::GRAY, kYSmall);

    buttons.setL1(Buttons::NONE);
    buttons.setL2(Buttons::NONE);
    buttons.setR1(Buttons::AMBER);
    buttons.setR2(Buttons::RED);
}

void MainView::renderConfirm(const IC::View& v)
{
    char buf[32];
    mMid.show("End session?", Col::WHITE, 70);
    mUnit.show("Top right: end", Col::RED, 118);
    mSmall.show("Bottom right: back", Col::WHITE, 146);
    const unsigned done = (v.underlying == IC::State::REST) ? v.rep : v.rep - 1u;
    std::snprintf(buf, sizeof buf, "%u of %u reps done", done, v.settings.reps);
    mFoot.show(buf, Col::GRAY, 180);

    buttons.setL1(Buttons::NONE);
    buttons.setL2(Buttons::NONE);
    buttons.setR1(Buttons::RED);
    buttons.setR2(Buttons::WHITE);
}

void MainView::renderSummaryRows()
{
    const IC::Summary& s = mSummary;
    char buf[64];
    char a[12];
    char b[12];

    std::snprintf(buf, sizeof buf, "%u/%u reps", s.repsDone, s.planned);
    mTitle.show(buf, kAmber, kYTitle);
    std::snprintf(buf, sizeof buf, "total %s", IC::formatDuration(s.totalMs / 1000u, a, sizeof a));
    mUnit.show(buf, Col::WHITE, 52);

    // Two reps per page: a rep line, then its rest line.
    const uint8_t first = mView.summaryScroll;
    static const int16_t kRowY[4] = {84, 108, 136, 160};
    int row = 0;
    for (uint8_t i = first; i < s.repsDone && row < 4; ++i) {
        const IC::RepResult& r = s.reps[i];
        IC::formatDuration(r.timeMs / 1000u, a, sizeof a);
        IC::formatPace(IC::paceSecPerKm(r.timeMs, r.distanceM), b, sizeof b);
        // A GPS gap marks the row red with "!" after the rep number (short:
        // a longer flag no longer fits the circle).
        const char* gap = r.gpsGap ? "!" : "";
        if (r.avgHr) {
            std::snprintf(buf, sizeof buf, "#%u%s  %s  %s/km  %u", i + 1u, gap, a, b, r.avgHr);
        } else {
            std::snprintf(buf, sizeof buf, "#%u%s  %s  %s/km  --", i + 1u, gap, a, b);
        }
        mRows[row].show(buf, r.gpsGap ? Col::RED : Col::WHITE, kRowY[row]);
        ++row;
        if (i < s.restsDone && row < 4) {
            IC::formatDuration(s.rests[i].lengthMs / 1000u, a, sizeof a);
            std::snprintf(buf, sizeof buf, "rest %s  %s", a, restReason(s.rests[i].reason));
            mRows[row].show(buf, kAmber, kRowY[row]);
            ++row;
        }
    }

    if (s.repsDone > 0) {
        const unsigned last = (first + 2u < s.repsDone) ? first + 2u : s.repsDone;
        std::snprintf(buf, sizeof buf, "%u-%u of %u", first + 1u, last, s.repsDone);
        mFoot.show(buf, Col::GRAY, 190);
    } else {
        mFoot.show("no reps finished", Col::GRAY, 120);
    }

    buttons.setL1(first > 0 ? Buttons::WHITE : Buttons::NONE);
    buttons.setL2(first + 1u < s.repsDone ? Buttons::WHITE : Buttons::NONE);
    buttons.setR1(Buttons::WHITE);
    buttons.setR2(Buttons::WHITE);
}
