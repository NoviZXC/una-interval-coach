#include <platform/hal/simulator/sdl2/HALSDL2.hpp>
#include <touchgfx/hal/NoDMA.hpp>
#include <common/TouchGFXInit.hpp>
#include <gui_generated/common/SimConstants.hpp>
#include <platform/driver/touch/SDL2TouchController.hpp>
#include <platform/driver/touch/NoTouchController.hpp>
#include <touchgfx/lcd/LCD.hpp>
#include <simulator/mainBase.hpp>
#include "touchgfx/Utils.hpp"

#include "SDK/Kernel/KernelBuilder.hpp"
#include "SDK/Simulator/Kernel/Kernel.hpp"
#include "SDK/Kernel/KernelProviderGUI.hpp"
#include "SDK/Kernel/KernelProviderService.hpp"
#include "SDK/Simulator/OS/OS.hpp"
#include "SDK/Simulator/App/AppCore.hpp"
#include "SDK/Simulator/App/AppMessageCore.hpp"
#include "SDK/Simulator/App/KernelMessageDispatcher.hpp"
#include "SDK/Simulator/Kernel/Mock/Backlight.hpp"
#include "SDK/Simulator/Kernel/Mock/Buzzer.hpp"
#include "SDK/Simulator/Kernel/Mock/Vibro.hpp"

#include "SDK/GUI/Config.hpp"

#include "Service.hpp"

#include <SDL2/SDL.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

#include <stdlib.h>
#include <thread>

#ifdef __linux__
#include <time.h>
#else
#include "Windows.h"
#endif

#define LOG_MODULE_PRX      "main"
#define LOG_MODULE_LEVEL    LOG_LEVEL_DEBUG
#include "SDK/UnaLogger/Logger.h"

using namespace touchgfx;

// Kernel thread function
static void kernelThreadFunction(App::Core* appCore)
{
    appCore->run();
}

// GUI communication thread 
static void guiCommThreadFunction(App::Core* appCore)
{
    appCore->runGuiComm();
}

// Service thread function
static void serviceThreadFunction(Service* service)
{
    service->run();
}

// KernelMessage thread function
static void appMessageThreadFunction(SDK::App::KernelMessageDispatcher* appMessage)
{
    appMessage->run();
}

// ---------------------------------------------------------------------------
// QA script mode (QA.md section B). With IC_QA_SCRIPT=<file>, a thread replays
// the file through SDL events, the same path as the keyboard:
//   wait <ms>      sleep
//   until <STATE> <timeout_ms>   wait for SETUP/GPS_WAIT/REP/PAUSED/REST/CONFIRM_END/SUMMARY
//   key <c>        press key c (1-4 buttons, 5 wrist, 6 QA GPS toggle)
//   shot <name>    screenshot to screenshots/<name>.bmp
//   quit           close the simulator
//   # ...          comment
// ---------------------------------------------------------------------------
std::atomic<char*> gIcQaShot{nullptr};
std::atomic<int>   gIcQaState{-1};

static int stateFromName(const std::string& s)
{
    static const char* const kNames[] = {"SETUP", "GPS_WAIT", "REP", "PAUSED", "REST", "CONFIRM_END", "SUMMARY"};
    for (int i = 0; i < 7; ++i) {
        if (s == kNames[i]) {
            return i;   // same order as IC::State
        }
    }
    return -1;
}

static void qaScriptThread(std::string path)
{
    std::ifstream in(path);
    if (!in) {
        LOG_ERROR("IC.Qa: cannot open script %s\n", path.c_str());
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));  // let the window come up

    std::string cmd;
    while (in >> cmd) {
        if (cmd[0] == '#') {
            std::getline(in, cmd);
        } else if (cmd == "wait") {
            long ms = 0;
            in >> ms;
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        } else if (cmd == "until") {
            // until <STATE> [timeout_ms]: wait for the GUI to show that state.
            std::string s;
            long timeout = 0;
            in >> s >> timeout;
            const int want = stateFromName(s);
            const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
            while (gIcQaState.load() != want && std::chrono::steady_clock::now() < end) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            if (gIcQaState.load() == want) {
                LOG_INFO("IC.Qa: until %s ok\n", s.c_str());
            } else {
                LOG_ERROR("IC.Qa: until %s TIMEOUT\n", s.c_str());
            }
        } else if (cmd == "key") {
            std::string k;
            in >> k;
            SDL_Event e{};
            e.type = SDL_TEXTINPUT;
            e.text.text[0] = k[0];
            SDL_PushEvent(&e);
            LOG_INFO("IC.Qa: key %c\n", k[0]);
            // The HAL keeps one pending key per frame: space presses out.
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        } else if (cmd == "shot") {
            // Taken by the GUI thread on its next tick (FrontendApplication),
            // straight to screenshots/<name>.bmp.
            std::string name;
            in >> name;
            // Let the last key's round trip (GUI -> service -> GUI) render first.
            std::this_thread::sleep_for(std::chrono::milliseconds(700));
            gIcQaShot.store(strdup((name + ".bmp").c_str()));
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            LOG_INFO("IC.Qa: shot %s\n", name.c_str());
        } else if (cmd == "quit") {
            LOG_INFO("IC.Qa: quit\n");
            SDL_Event e{};
            e.type = SDL_QUIT;
            const int pushed = SDL_PushEvent(&e);
            SDL_Event w{};
            w.type = SDL_WINDOWEVENT;
            w.window.event = SDL_WINDOWEVENT_CLOSE;
            SDL_PushEvent(&w);
            LOG_INFO("IC.Qa: quit pushed=%d %s\n", pushed, SDL_GetError());
            return;
        }
    }
}

static int runTouchGFX(SDK::App::DualAppComm&  appComm,
                       SDK::Simulator::Kernel& srvKernel,
                       SDK::Simulator::Kernel& guiKernel,
                       int                     argc,
                       char**                  argv)
{
    // Initialize Logger with Service's kernel. In real app Service and GUI will have each its own kernel.
    Logger_init(srvKernel.getKernel().log);

    // Save Service's kernel for global access
    SDK::KernelProviderService::CreateInstance(&srvKernel.getKernel());

    // Save GUI's kernel for global access
    SDK::KernelProviderGUI::CreateInstance(&guiKernel.getKernel());

    // Create the Service of the application
    Service service(SDK::KernelProviderService::GetInstance().getKernel());

    // Create KernelMessageDispatcher core
    SDK::Simulator::Mock::Backlight   mBacklight;
    SDK::Simulator::Mock::Buzzer      mBuzzer;
    SDK::Simulator::Mock::Vibro       mVibro;
    SDK::App::KernelMessageDispatcher kernelMessage(appComm, appComm.getMsgManager(), mVibro, mBacklight, mBuzzer);

	// Create the Application core
	App::Core appCore(appComm, srvKernel, guiKernel);

    //For windows/linux, DMA transfers are simulated
    touchgfx::NoDMA dma;
    LCD& lcd = setupLCD();
    touchgfx::NoTouchController tc;

    touchgfx::HAL& hal = touchgfx::touchgfx_generic_init<touchgfx::HALSDL2>(dma, lcd, tc, SIM_WIDTH, SIM_HEIGHT, 0, 0);

    setupSimulator(argc, argv, hal);

    // Set custom frame rate
    static_cast<touchgfx::HALSDL2&>(hal).setVsyncInterval(1000.0f / SDK::GUI::Config::kFrameRate);

    //// Ensure there is a console window to print to using printf() or
    //// std::cout, and read from using e.g. fgets or std::cin.
    //// Alternatively, instead of using printf(), always use
    //// touchgfx_printf() which will ensure there is a console to write
    //// to.
    touchgfx_enable_stdio();

    // QA script mode: the HAL points stdout at its own (hidden) console, so
    // send it to a file instead, unbuffered, for check_log.py.
    if (const char* log = std::getenv("IC_QA_LOG")) {
        std::freopen(log, "w", stdout);
        std::setvbuf(stdout, nullptr, _IONBF, 0);
    }

    // Start service thread
    std::thread serviceThread(serviceThreadFunction, &service);
    std::thread appThread(kernelThreadFunction, &appCore);
    std::thread guiCommThread(guiCommThreadFunction, &appCore);
    std::thread appMessageThread(appMessageThreadFunction, &kernelMessage);

    std::thread qaThread;
    if (const char* script = std::getenv("IC_QA_SCRIPT")) {
        qaThread = std::thread(qaScriptThread, std::string(script));
    }

    touchgfx::HAL::getInstance()->taskEntry();  // Main GUI loop
	
    appCore.stopRequest();

    // Stop threads
    serviceThread.join();
    appThread.join();
    guiCommThread.join();
    appMessageThread.join();
    if (qaThread.joinable()) {
        qaThread.join();
    }

    return EXIT_SUCCESS;
}

#ifdef __linux__
int main(int argc, char** argv)
{
#else
#include <shellapi.h>
#ifdef _UNICODE
#error Cannot run in unicode mode
#endif
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    int argc;
    char** argv = touchgfx::HALSDL2::getArgv(&argc);
#endif

    // Create kernel objects and service control
    SDK::App::MessageCore         appMessageCore;

	SDK::Simulator::Mock::SystemService serviceSystem;
    SDK::Simulator::Kernel serviceKernel("service");
	serviceKernel.setIAppComm(appMessageCore.getAppComm().getServiceComm());
	serviceKernel.setISystem(&serviceSystem);

    SDK::Simulator::Mock::SystemGUI guiSystem;
    SDK::Simulator::Kernel guiKernel("gui");
    guiKernel.setIAppComm(appMessageCore.getAppComm().getGuiComm());
    guiKernel.setISystem(&guiSystem);

    // Save GUI kernel for simulator
    SDK::Simulator::KernelHolder::Create(guiKernel);

    return runTouchGFX(appMessageCore.getAppComm(),
                       serviceKernel,
                       guiKernel,
                       argc,
                       argv);
}
