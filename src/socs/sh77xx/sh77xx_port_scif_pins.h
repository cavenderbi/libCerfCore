#pragma once

#include "sh77xx_scif_pins.h"

namespace cerfcore {

class Sh77xxIntc;
class Sh77xxPortData;

class Sh77xxPortScifPins : public Sh77xxScifPins {
public:
    using Sh77xxScifPins::Sh77xxScifPins;

    void OnReady() override;

    bool TxdLow(const Sh77xxScifChannel& channel) const override;
    bool CtsHigh(const Sh77xxScifChannel& channel) const override;
    void PublishEnables(const Sh77xxScifChannel& channel, bool transmit, bool receive) override;
    void DriveRts(const Sh77xxScifChannel& channel, bool high) override;
    void AddPinListener(std::function<void()> listener) override;

private:
    Sh77xxPortData* ports_ = nullptr;
    Sh77xxIntc*     intc_  = nullptr;
};

}
