#pragma once

#include <cerfcore/core/callback_list.h>
#include <cerfcore/core/service.h>

#include <functional>
#include <utility>

namespace cerfcore {

class Sh7707LcdPanel : public Service {
public:
    using Service::Service;

    virtual bool ShowsColor() const          = 0;
    virtual bool TakesLowerPanelData() const = 0;
    virtual bool Lit() const                 = 0;

    void AddInputListener(std::function<void()> listener) { input_listeners_.Add(std::move(listener)); }

protected:
    void NotifyInputsChanged() { input_listeners_.Call(); }

private:
    CallbackList<> input_listeners_;
};

}
