#ifndef MAINVIEW_HPP
#define MAINVIEW_HPP

#include <gui_generated/main_screen/MainViewBase.hpp>
#include <gui/main_screen/MainPresenter.hpp>

#include <touchgfx/widgets/TextAreaWithWildcard.hpp>
#include <texts/TextKeysAndLanguages.hpp>

/**
 * Every SPEC.md screen, drawn from the View the service sends. Content stays
 * inside radius 108 of the 240x240 round display; the button hints sit on
 * the bezel arcs (radius 113).
 */
class MainView : public MainViewBase
{
public:
    MainView();
    virtual ~MainView() {}

    virtual void setupScreen() override;
    virtual void tearDownScreen() override;
    virtual void handleKeyEvent(uint8_t key) override;

    void render(const IC::View& v);
    void renderSummary(const IC::Summary& s);

private:
    /// A one-line, horizontally centred (or left/right aligned) text.
    struct Label {
        static constexpr uint16_t kLen = 40;
        touchgfx::TextAreaWithOneWildcard text;
        touchgfx::Unicode::UnicodeChar    buf[kLen] = {};

        void init(TypedTextId id, int16_t y, int16_t h, int16_t x = 0, int16_t w = 240);
        void show(const char* s, uint32_t rgb, int16_t y);
        void hide();
    };

    void renderSetup(const IC::View& v);
    void renderGpsWait(const IC::View& v);
    void renderRep(const IC::View& v);
    void renderRest(const IC::View& v);
    void renderConfirm(const IC::View& v);
    void renderSummaryRows();
    void hideAll();

    Label mTitle;   ///< SemiBold 20, top
    Label mName;    ///< Medium 25, setup field name
    Label mBig;     ///< SemiBold 60, the one primary value
    Label mUnit;    ///< Regular 18, under the primary value
    Label mMid;     ///< SemiBold 30, secondary value
    Label mSmall;   ///< Regular 18, bottom line
    Label mRows[4]; ///< Regular 18, summary rows
    Label mFoot;    ///< Regular 18, summary position
    Label mLeft;    ///< Regular 18, right-aligned left half (pace)
    Label mRight;   ///< Regular 18, left-aligned right half (HR)

    IC::View    mView{};
    IC::Summary mSummary{};
};

#endif // MAINVIEW_HPP
