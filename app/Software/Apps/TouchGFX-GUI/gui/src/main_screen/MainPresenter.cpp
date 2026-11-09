#include <gui/main_screen/MainView.hpp>
#include <gui/main_screen/MainPresenter.hpp>

MainPresenter::MainPresenter(MainView& v)
    : view(v)
{
}

void MainPresenter::activate()
{
    view.render(model->view());
    view.renderSummary(model->summary());
}

void MainPresenter::deactivate()
{
}

void MainPresenter::onView(const IC::View& v)
{
    view.render(v);
}

void MainPresenter::onSummary(const IC::Summary& s)
{
    view.renderSummary(s);
}

void MainPresenter::button(IC::Button b)
{
    model->sendButton(b);
}
