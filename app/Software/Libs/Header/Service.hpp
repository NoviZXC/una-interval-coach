/**
 * @file    Service.hpp
 * @brief   Interval Coach service: sensors in, engine events out to the
 *          vibration motor, buzzer, backlight and GUI. No session rules here;
 *          they all live in IC::IntervalEngine.
 */
#ifndef SERVICE_HPP
#define SERVICE_HPP

#include "SDK/Kernel/Kernel.hpp"
#include "SDK/Messages/CommandMessages.hpp"
#include "SDK/SensorLayer/SensorConnection.hpp"
#include "SDK/SensorLayer/SensorDataBatch.hpp"

#include "Commands.hpp"
#include "IntervalEngine.hpp"
#include "SettingsStore.hpp"

class Service
{
public:
    explicit Service(SDK::Kernel& kernel);
    ~Service();

    void run();

private:
    static constexpr uint32_t kSamplePeriodMs = 1000;
    static constexpr uint32_t kLatencyMs      = 1000;
    static constexpr uint32_t kTickMs         = 1000;
    static constexpr uint32_t kBacklightMs    = 5000;
    static constexpr uint32_t kGraceMs        = 5000;  ///< Startup grace (service-lifecycle §5.2).

    using Vibro = SDK::Message::RequestVibroPlay;

    SDK::Kernel&       mKernel;
    SettingsStore      mStore;
    IC::IntervalEngine mEngine;

    SDK::Sensor::Connection mGpsLocation;
    SDK::Sensor::Connection mGpsSpeed;
    SDK::Sensor::Connection mGpsDistance;
    SDK::Sensor::Connection mHeartRate;
    bool mSensorsOn = false;

    bool  mGuiUp       = false;
    bool  mGuiEverUp   = false;
    bool  mGuiOnScreen = false;
    bool  mFix         = false;
    float mSpeed       = 0.0f;
    bool  mQaGpsOff    = false;   ///< Simulator fault hooks only (QA.md B).
    bool  mQaHrOff     = false;
    bool  mQaHrHigh    = false;

    uint32_t mStartMs    = 0;
    uint32_t mLastTickMs = 0;
    uint32_t mLastViewMs = 0;
    bool     mDirty      = true;
    bool     mSummarySent = false;

    uint32_t now() const;

    void onGuiRun();
    void onGuiStop();
    void onGuiFocus(bool onScreen);
    void updateSensors();
    void connectSensors();
    void disconnectSensors();
    void handleSensors(uint16_t handle, SDK::Sensor::DataBatch& data);

    void dispatchEvents();
    void onEvent(IC::Event e);
    void sendView();
    void sendSummary();

    void backlightOn();
    void beep(uint16_t ms);
    void vibrate(const Vibro::Note* notes, uint8_t count);
};

#endif // SERVICE_HPP
