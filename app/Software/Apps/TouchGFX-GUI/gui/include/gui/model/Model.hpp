#ifndef MODEL_HPP
#define MODEL_HPP

#include "touchgfx/UIEventListener.hpp"

#include "SDK/Kernel/Kernel.hpp"
#include "SDK/Interfaces/IGuiLifeCycleCallback.hpp"
#include "SDK/Interfaces/ICustomMessageHandler.hpp"

#include "Commands.hpp"

class FrontendApplication;
class ModelListener;

/**
 * GUI side of Interval Coach. Holds the last View/Summary the service sent
 * and forwards buttons to the service. No session logic here.
 */
class Model : public touchgfx::UIEventListener,
              public SDK::Interface::IGuiLifeCycleCallback,
              public SDK::Interface::ICustomMessageHandler
{
public:
    Model();

    void bind(ModelListener* listener) { modelListener = listener; }

    void tick();
    void sendButton(IC::Button b);
#if defined(SIMULATOR)
    void qaFault(uint8_t code);   ///< Simulator fault hook (QA.md B), keys 6/7/8.
#endif
    void exitApp();

    const IC::View&    view() const { return mView; }
    const IC::Summary& summary() const { return mSummary; }

    // IGuiLifeCycleCallback
    void onStart() override;
    void onResume() override;
    void onSuspend() override;
    void onStop() override;

    // ICustomMessageHandler
    bool customMessageHandler(SDK::MessageBase* message) override;

protected:
    ModelListener* modelListener;

private:
    const SDK::Kernel& mKernel;
    IC::View     mView{};
    IC::Summary  mSummary{};
};

#endif // MODEL_HPP
