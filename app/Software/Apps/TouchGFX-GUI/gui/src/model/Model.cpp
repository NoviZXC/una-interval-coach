#include <gui/model/Model.hpp>
#include <gui/model/ModelListener.hpp>

#include "SDK/Kernel/KernelProviderGUI.hpp"
#include "SDK/Messages/MessageGuard.hpp"
#include "SDK/Port/TouchGFX/TouchGFXCommandProcessor.hpp"

#if defined(SIMULATOR)
#include <atomic>
/// Last state shown, for the QA script's "until" command (simulator/main.cpp).
extern std::atomic<int> gIcQaState;
#endif

#define LOG_MODULE_PRX      "IC.Gui"
#define LOG_MODULE_LEVEL    LOG_LEVEL_INFO
#include "SDK/UnaLogger/Logger.h"

Model::Model()
    : modelListener(nullptr)
    , mKernel(SDK::KernelProviderGUI::GetInstance().getKernel())
{
    SDK::TouchGFXCommandProcessor::GetInstance().setAppLifeCycleCallback(this);
    SDK::TouchGFXCommandProcessor::GetInstance().setCustomMessageHandler(this);
#if defined(SIMULATOR)
    LOG_INFO("Buttons: 1=L1 (up) 2=L2 (down) 3=R1 (select) 4=R2 (back); QA: 6=GPS off/on 7=HR off/on 8=HR +60\n");
#endif
}

void Model::tick()
{
}

void Model::sendButton(IC::Button b)
{
    SDK::send_msg<CustomMessage::ButtonPress>(mKernel, b);
}

#if defined(SIMULATOR)
void Model::qaFault(uint8_t code)
{
    SDK::send_msg<CustomMessage::QaFault>(mKernel, code);
}
#endif

void Model::exitApp()
{
    LOG_INFO("exit\n");
    SDK::TouchGFXCommandProcessor::GetInstance().setAppLifeCycleCallback(nullptr);
    SDK::TouchGFXCommandProcessor::GetInstance().setCustomMessageHandler(nullptr);
    mKernel.sys.exit();
}

void Model::onStart()
{
    LOG_INFO("started\n");
}

void Model::onResume()
{
    // The service is not told about focus; tell it, so it can keep sensors
    // off while nobody looks at Setup (service-lifecycle.md §6).
    SDK::send_msg<CustomMessage::GuiFocus>(mKernel, true);
}

void Model::onSuspend()
{
    SDK::send_msg<CustomMessage::GuiFocus>(mKernel, false);
}

void Model::onStop()
{
    LOG_INFO("stopped\n");
}

bool Model::customMessageHandler(SDK::MessageBase* message)
{
    switch (message->getType()) {
    case CustomMessage::VIEW_UPDATE:
        mView = static_cast<CustomMessage::ViewUpd*>(message)->view;
#if defined(SIMULATOR)
        gIcQaState.store(static_cast<int>(mView.state));
#endif
        if (modelListener) {
            modelListener->onView(mView);
        }
        break;

    case CustomMessage::SUMMARY_UPDATE:
        mSummary = static_cast<CustomMessage::SummaryUpd*>(message)->summary;
        if (modelListener) {
            modelListener->onSummary(mSummary);
        }
        break;

    case CustomMessage::APP_EXIT:
        exitApp();
        break;

    default:
        break;
    }
    return true;
}
