/**
 * @file    Commands.hpp
 * @brief   Custom messages between the Interval Coach service and GUI.
 *
 * The service owns the session (IntervalEngine). The GUI forwards buttons and
 * draws whatever View the service last sent.
 */
#ifndef COMMANDS_HPP
#define COMMANDS_HPP

#include "SDK/Messages/MessageBase.hpp"
#include "SDK/Messages/MessageTypes.hpp"

#include "IntervalEngine.hpp"

#pragma pack(push, 4)

namespace CustomMessage {

// Service --> GUI
constexpr SDK::MessageType::Type VIEW_UPDATE    = 0x00000001;
constexpr SDK::MessageType::Type SUMMARY_UPDATE = 0x00000002;
constexpr SDK::MessageType::Type APP_EXIT       = 0x00000003;

// GUI --> Service
constexpr SDK::MessageType::Type BUTTON         = 0x00000010;
constexpr SDK::MessageType::Type GUI_FOCUS      = 0x00000011;
constexpr SDK::MessageType::Type QA_FAULT       = 0x00000012;  ///< Simulator-only fault hook (QA.md B).

struct ViewUpd : public SDK::MessageBase {
    IC::View view;
    ViewUpd() : SDK::MessageBase(VIEW_UPDATE), view{} {}
    explicit ViewUpd(const IC::View& v) : ViewUpd() { view = v; }
};

struct SummaryUpd : public SDK::MessageBase {
    IC::Summary summary;
    SummaryUpd() : SDK::MessageBase(SUMMARY_UPDATE), summary{} {}
    explicit SummaryUpd(const IC::Summary& s) : SummaryUpd() { summary = s; }
};

struct AppExit : public SDK::MessageBase {
    AppExit() : SDK::MessageBase(APP_EXIT) {}
};

struct ButtonPress : public SDK::MessageBase {
    IC::Button button;
    ButtonPress() : SDK::MessageBase(BUTTON), button(IC::Button::L1) {}
    explicit ButtonPress(IC::Button b) : ButtonPress() { button = b; }
};

/// The GUI is (or is no longer) the app on screen. The service is not told
/// about suspend/resume itself (Docs/service-lifecycle.md §6).
struct GuiFocus : public SDK::MessageBase {
    bool onScreen;
    GuiFocus() : SDK::MessageBase(GUI_FOCUS), onScreen(true) {}
    explicit GuiFocus(bool on) : GuiFocus() { onScreen = on; }
};

/// Simulator keys 6/7/8 toggle a fault in the service's sensor input.
struct QaFault : public SDK::MessageBase {
    enum Code : uint8_t { GPS_OFF = 6, HR_OFF = 7, HR_HIGH = 8 };
    uint8_t code;
    QaFault() : SDK::MessageBase(QA_FAULT), code(0) {}
    explicit QaFault(uint8_t c) : QaFault() { code = c; }
};

} // namespace CustomMessage

#pragma pack(pop)

#endif // COMMANDS_HPP
