#include "../sh77xx/sh77xx_scif_pins.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/callback_list.h>
#include <cerfcore/core/cerf_emulator.h>
#include "hd6417096_id.h"
#include "hd6417096_scif_cts_input.h"
#include "hd6417096_scif_port_pins.h"

#include <atomic>
#include <utility>

namespace cerfcore {

namespace {

class Hd6417096ScifPins : public Sh77xxScifPins {
public:
    using Sh77xxScifPins::Sh77xxScifPins;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Hd6417096; }

    void OnReady() override {
        port_pins_ = &emu_.Get<Hd6417096ScifPortPins>();
        cts_       = &emu_.Get<Hd6417096ScifCtsInput>();
    }

    bool TxdLow(const Sh77xxScifChannel&) const override {
        return !transmit_.load(std::memory_order_acquire) && port_pins_->TxdPortLow();
    }

    bool CtsHigh(const Sh77xxScifChannel&) const override { return cts_->High(); }

    void PublishEnables(const Sh77xxScifChannel&, bool transmit, bool) override {
        if (transmit_.exchange(transmit, std::memory_order_acq_rel) != transmit) listeners_.Call();
    }

    void DriveRts(const Sh77xxScifChannel&, bool high) override { port_pins_->SetRtsFunctionHigh(high); }

    void AddPinListener(std::function<void()> listener) override {
        port_pins_->AddChangeListener(listener);
        cts_->AddChangeListener(listener);
        listeners_.Add(std::move(listener));
    }

private:
    Hd6417096ScifPortPins* port_pins_ = nullptr;
    Hd6417096ScifCtsInput* cts_       = nullptr;
    std::atomic<bool>      transmit_{false};
    CallbackList<>         listeners_;
};

}

REGISTER_SERVICE_AS(Hd6417096ScifPins, Sh77xxScifPins);

}
