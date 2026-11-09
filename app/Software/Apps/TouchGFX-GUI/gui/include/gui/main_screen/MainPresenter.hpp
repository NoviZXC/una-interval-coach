#ifndef MAINPRESENTER_HPP
#define MAINPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class MainView;

class MainPresenter : public touchgfx::Presenter, public ModelListener
{
public:
    explicit MainPresenter(MainView& v);
    virtual ~MainPresenter() {}

    virtual void activate() override;
    virtual void deactivate() override;

    virtual void onView(const IC::View& view) override;
    virtual void onSummary(const IC::Summary& summary) override;

    void button(IC::Button b);

private:
    MainPresenter();
    MainView& view;
};

#endif // MAINPRESENTER_HPP
