#ifndef FRONTENDAPPLICATION_HPP
#define FRONTENDAPPLICATION_HPP

#include <gui_generated/common/FrontendApplicationBase.hpp>

#include "SDK/Port/TouchGFX/TouchGFXCommandProcessor.hpp"

#if defined(SIMULATOR)
#include <atomic>
#include <cstdlib>
#include <platform/hal/simulator/sdl2/HALSDL2.hpp>
#include "SDK/Simulator/Kernel/Kernel.hpp"
#include "SDK/Simulator/Components/InstanceSensorLayer.hpp"
/// QA script mode (simulator/main.cpp): file name of a screenshot to take on
/// the next GUI tick, malloc'd; nullptr when none is pending.
extern std::atomic<char*> gIcQaShot;
#endif

class FrontendHeap;

using namespace touchgfx;

class FrontendApplication : public FrontendApplicationBase
{
public:
    FrontendApplication(Model& m, FrontendHeap& heap);
    virtual ~FrontendApplication() { }

    virtual void handleTickEvent()
    {
#if defined(SIMULATOR)
        SDK::Simulator::KernelHolder::Get().tick();
        if (char* shot = gIcQaShot.exchange(nullptr)) {
            static_cast<touchgfx::HALSDL2*>(touchgfx::HAL::getInstance())->saveScreenshot(nullptr, shot);
            std::free(shot);
        }
#endif
        SDK::TouchGFXCommandProcessor::GetInstance().callCustomMessageHandler();
        model.tick();
        FrontendApplicationBase::handleTickEvent();
    }

    void handleKeyEvent(uint8_t key)
    {
#if defined(SIMULATOR)
        Instance::SensorLayer::getInstance().handlerButtons(key);
        if (key >= '6' && key <= '8') {   // QA fault hooks; the kernel filter drops non-button keys
            model.qaFault(static_cast<uint8_t>(key - '0'));
            return;
        }
        if (SDK::Simulator::KernelHolder::Get().keyFilter(key)) {
            FrontendApplicationBase::handleKeyEvent(key);
        }
#else
        FrontendApplicationBase::handleKeyEvent(key);
#endif
    }
};

#endif // FRONTENDAPPLICATION_HPP
