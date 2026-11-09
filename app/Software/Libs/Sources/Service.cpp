/**
 * @file    Service.cpp
 * @brief   Interval Coach service. See Service.hpp, SPEC.md §4 (alerts) and
 *          §9 (lifecycle).
 */
#include "Service.hpp"

#include "SDK/Messages/MessageGuard.hpp"
#include "SDK/Messages/SensorLayerMessages.hpp"
#include "SDK/SensorLayer/DataParsers/SensorDataParserGpsDistance.hpp"
#include "SDK/SensorLayer/DataParsers/SensorDataParserGpsLocation.hpp"
#include "SDK/SensorLayer/DataParsers/SensorDataParserGpsSpeed.hpp"
#include "SDK/SensorLayer/DataParsers/SensorDataParserHeartRate.hpp"

#include "Lifecycle.hpp"

#define LOG_MODULE_PRX      "IC"
#define LOG_MODULE_LEVEL    LOG_LEVEL_INFO
#include "SDK/UnaLogger/Logger.h"

namespace {

using Vibro = SDK::Message::RequestVibroPlay;

constexpr Vibro::Note fx(Vibro::Effect e) { return Vibro::Note{static_cast<uint8_t>(e), 0}; }
constexpr Vibro::Note gap(uint32_t ms)    { return Vibro::Note{static_cast<uint8_t>(Vibro::NO_EFFECT), ms}; }

// SPEC.md §4. Long = stop, short = go.
constexpr Vibro::Note kRepDone[]    = {fx(Vibro::ALERT_1000MS_100), gap(300), fx(Vibro::ALERT_1000MS_100)};
constexpr Vibro::Note kGo[]         = {fx(Vibro::STRONG_CLICK_100), gap(150), fx(Vibro::STRONG_CLICK_100),
                                       gap(150), fx(Vibro::STRONG_CLICK_100)};
constexpr Vibro::Note kSessionDone[] = {fx(Vibro::ALERT_1000MS_100), gap(300), fx(Vibro::STRONG_CLICK_100),
                                        gap(150), fx(Vibro::STRONG_CLICK_100), gap(150),
                                        fx(Vibro::STRONG_CLICK_100)};
constexpr Vibro::Note kBump[]       = {fx(Vibro::SOFT_BUMP_100)};

template <std::size_t N>
constexpr uint8_t len(const Vibro::Note (&)[N]) { return static_cast<uint8_t>(N); }

const char* eventName(IC::Event e)
{
    switch (e) {
    case IC::Event::REP_DONE:           return "REP_DONE";
    case IC::Event::GO:                 return "GO";
    case IC::Event::SESSION_DONE:       return "SESSION_DONE";
    case IC::Event::GPS_LOST:           return "GPS_LOST";
    case IC::Event::GPS_BACK:           return "GPS_BACK";
    case IC::Event::PAUSED:             return "PAUSED";
    case IC::Event::RESUMED:            return "RESUMED";
    case IC::Event::SETTINGS_COMMITTED: return "SETTINGS_COMMITTED";
    case IC::Event::EXIT_APP:           return "EXIT_APP";
    }
    return "?";
}

} // namespace

Service::Service(SDK::Kernel& kernel)
    : mKernel(kernel)
    , mStore(kernel, "settings.json")
    , mEngine()
    , mGpsLocation(SDK::Sensor::Type::GPS_LOCATION, kSamplePeriodMs, kLatencyMs)
    , mGpsSpeed(SDK::Sensor::Type::GPS_SPEED, kSamplePeriodMs, kLatencyMs)
    , mGpsDistance(SDK::Sensor::Type::GPS_DISTANCE, kSamplePeriodMs, kLatencyMs)
    , mHeartRate(SDK::Sensor::Type::HEART_RATE, kSamplePeriodMs, kLatencyMs)
{}

Service::~Service()
{
    disconnectSensors();
}

uint32_t Service::now() const
{
    return mKernel.sys.getTimeMs();
}

void Service::run()
{
    IC::Settings settings;
    if (mStore.load(settings)) {
        LOG_INFO("settings loaded: reps=%u dist=%u hr=%u rest=%u\n",
                 settings.reps, settings.repDistanceM, settings.hrTarget, settings.maxRestS);
    } else {
        LOG_INFO("no saved settings, using defaults\n");
    }
    mEngine.setSettings(settings);

    mStartMs = mLastTickMs = now();

    while (true) {
        SDK::MessageBase* msg = nullptr;
        if (mKernel.comm.getMessage(msg, 200)) {
            switch (msg->getType()) {
            case SDK::MessageType::COMMAND_APP_STOP:
                LOG_INFO("IC.Life: APP_STOP\n");
                disconnectSensors();
                mKernel.comm.releaseMessage(msg);
                return;

            case SDK::MessageType::COMMAND_APP_NOTIF_GUI_RUN:
                onGuiRun();
                break;

            case SDK::MessageType::COMMAND_APP_NOTIF_GUI_STOP:
                onGuiStop();
                break;

            case CustomMessage::BUTTON:
                mEngine.onButton(now(), static_cast<CustomMessage::ButtonPress*>(msg)->button);
                mDirty = true;
                break;

            case CustomMessage::GUI_FOCUS:
                onGuiFocus(static_cast<CustomMessage::GuiFocus*>(msg)->onScreen);
                break;

#if defined(SIMULATOR)
            case CustomMessage::QA_FAULT: {
                const uint8_t code = static_cast<CustomMessage::QaFault*>(msg)->code;
                bool& flag = code == CustomMessage::QaFault::GPS_OFF ? mQaGpsOff
                           : code == CustomMessage::QaFault::HR_OFF  ? mQaHrOff : mQaHrHigh;
                flag = !flag;
                LOG_INFO("IC.Qa: fault %u %s\n", code, flag ? "ON" : "OFF");
            } break;
#endif

            case SDK::MessageType::EVENT_SENSOR_LAYER_DATA: {
                auto* event = static_cast<SDK::Message::Sensor::EventData*>(msg);
                SDK::Sensor::DataBatch batch(event->data, event->count, event->stride);
                handleSensors(event->handle, batch);
            } break;

            default:
                break;
            }
            mKernel.comm.releaseMessage(msg);
        }

        const uint32_t t = now();
        if (t - mLastTickMs >= kTickMs) {
            mLastTickMs = t;
            mEngine.onTick(t);
            mDirty = true;
        }

        dispatchEvents();
        updateSensors();

        if (mGuiUp && (mDirty || t - mLastViewMs >= kTickMs)) {
            sendView();
        }

        if (IC::shouldExit(mEngine.sessionActive(), mGuiUp, t - mStartMs >= kGraceMs)) {
            LOG_INFO("IC.Life: no GUI and no session, exiting\n");
            disconnectSensors();
            return;
        }
    }
}

// ------------------------------------------------------------ GUI lifecycle

void Service::onGuiRun()
{
    mGuiUp = mGuiEverUp = true;
    mGuiOnScreen = true;
    LOG_INFO("IC.Life: GUI_RUN, session %s\n", mEngine.sessionActive() ? "active" : "idle");
    // A returning GUI gets the full current state (SPEC.md §9).
    sendView();
    if (mEngine.state() == IC::State::SUMMARY) {
        sendSummary();
    }
}

void Service::onGuiStop()
{
    mGuiUp = false;
    mGuiOnScreen = false;
    if (mEngine.sessionActive()) {
        LOG_INFO("IC.Life: GUI_STOP, session active, staying\n");
    } else {
        LOG_INFO("IC.Life: GUI_STOP, no session\n");
    }
}

void Service::onGuiFocus(bool onScreen)
{
    mGuiOnScreen = onScreen;
    LOG_INFO("IC.Life: GUI %s\n", onScreen ? "on screen" : "off screen");
    if (onScreen) {
        mDirty = true;
    }
}

// ------------------------------------------------------------ Sensors

void Service::updateSensors()
{
    // GPS and HR while a session runs, or while the GUI is on screen (GPS
    // wait needs the fix). Nothing otherwise, to save battery.
    const bool want = mEngine.sessionActive() || (mGuiUp && mGuiOnScreen);
    if (want && !mSensorsOn) {
        connectSensors();
    } else if (!want && mSensorsOn) {
        disconnectSensors();
    }
}

void Service::connectSensors()
{
    mGpsLocation.connect();
    mGpsSpeed.connect();
    mGpsDistance.connect();
    mHeartRate.connect();
    mSensorsOn = true;
    LOG_INFO("sensors connected\n");
}

void Service::disconnectSensors()
{
    if (!mSensorsOn) {
        return;
    }
    mHeartRate.disconnect();
    mGpsDistance.disconnect();
    mGpsSpeed.disconnect();
    mGpsLocation.disconnect();
    mSensorsOn = false;
    mFix = false;
    LOG_INFO("sensors disconnected\n");
}

void Service::handleSensors(uint16_t handle, SDK::Sensor::DataBatch& data)
{
    const uint32_t t = now();

    if (mGpsLocation.matchesDriver(handle)) {
        SDK::SensorDataParser::GpsLocation p(data[0]);
        if (p.isDataValid()) {
            mFix = p.isCoordinatesValid() && !mQaGpsOff;
        }
    } else if (mGpsSpeed.matchesDriver(handle)) {
        SDK::SensorDataParser::GpsSpeed p(data[0]);
        if (p.isDataValid() && p.isSpeedValid()) {
            mSpeed = p.getSpeed();
        }
    } else if (mGpsDistance.matchesDriver(handle)) {
        // One engine GPS sample per odometer update (1 Hz), with the latest
        // fix and speed.
        SDK::SensorDataParser::GpsDistance p(data[0]);
        if (p.isDataValid()) {
            mEngine.onGps(t, mFix && !mQaGpsOff, mSpeed, p.getDistance());
            mDirty = true;
        }
    } else if (mHeartRate.matchesDriver(handle)) {
        SDK::SensorDataParser::HeartRate p(data[0]);
        if (p.isDataValid() && !mQaHrOff) {
            const float bpm = p.getBpm() + (mQaHrHigh ? 60.0f : 0.0f);
            mEngine.onHeartRate(t, bpm, static_cast<uint8_t>(p.getTrustLevel()));
            mDirty = true;
        }
    }
}

// ------------------------------------------------------------ Events

void Service::dispatchEvents()
{
    IC::Event events[IC::IntervalEngine::kMaxEvents];
    const std::size_t n = mEngine.takeEvents(events, IC::IntervalEngine::kMaxEvents);
    for (std::size_t i = 0; i < n; ++i) {
        onEvent(events[i]);
    }
    if (n) {
        mDirty = true;
    }
}

void Service::onEvent(IC::Event e)
{
    const IC::View v = mEngine.view(now());
    LOG_INFO("IC.Event: %s rep=%u\n", eventName(e), v.rep);

    switch (e) {
    case IC::Event::REP_DONE:
        backlightOn();
        vibrate(kRepDone, len(kRepDone));
        beep(200);
        break;
    case IC::Event::GO:
        backlightOn();
        vibrate(kGo, len(kGo));
        beep(200);
        break;
    case IC::Event::SESSION_DONE:
        backlightOn();
        vibrate(kSessionDone, len(kSessionDone));
        break;
    case IC::Event::GPS_LOST:
    case IC::Event::GPS_BACK:
    case IC::Event::PAUSED:
    case IC::Event::RESUMED:
        backlightOn();
        vibrate(kBump, len(kBump));
        break;
    case IC::Event::SETTINGS_COMMITTED:
        mStore.save(mEngine.settings());
        break;
    case IC::Event::EXIT_APP:
        SDK::send_msg<CustomMessage::AppExit>(mKernel);
        break;
    }
}

void Service::sendView()
{
    const uint32_t t = now();
    // The summary goes once per entry into SUMMARY (last rep or early end),
    // before the view that shows it.
    const bool inSummary = (mEngine.state() == IC::State::SUMMARY);
    if (inSummary && !mSummarySent) {
        sendSummary();
    }
    mSummarySent = inSummary && mSummarySent;
    SDK::send_msg<CustomMessage::ViewUpd>(mKernel, mEngine.view(t));
    mLastViewMs = t;
    mDirty = false;
}

void Service::sendSummary()
{
    if (mGuiUp) {
        SDK::send_msg<CustomMessage::SummaryUpd>(mKernel, mEngine.summary());
        mSummarySent = true;
    }
}

// ------------------------------------------------------------ Outputs

void Service::backlightOn()
{
    auto bl = SDK::make_msg<SDK::Message::RequestBacklightSet>(mKernel);
    if (bl) {
        bl->brightness       = 100;
        bl->autoOffTimeoutMs = kBacklightMs;
        bl.send();
    }
}

void Service::beep(uint16_t ms)
{
    auto* msg = mKernel.comm.allocateMessage<SDK::Message::RequestBuzzerPlay>();
    if (msg) {
        msg->notes[0].time   = ms;
        msg->notes[0].volume = 100;
        msg->notesCount      = 1;
        mKernel.comm.sendMessage(msg);
        mKernel.comm.releaseMessage(msg);
    }
}

void Service::vibrate(const Vibro::Note* notes, uint8_t count)
{
    auto* msg = mKernel.comm.allocateMessage<Vibro>();
    if (msg) {
        for (uint8_t i = 0; i < count && i < Vibro::skMaxNotes; ++i) {
            msg->notes[i] = notes[i];
        }
        msg->notesCount = count;
        mKernel.comm.sendMessage(msg);
        mKernel.comm.releaseMessage(msg);
    }
}
