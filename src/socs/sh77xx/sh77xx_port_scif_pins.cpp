#include "sh77xx_port_scif_pins.h"

#include <cerfcore/core/cerf_emulator.h>
#include "sh77xx_intc.h"
#include "sh77xx_pfc.h"
#include "sh77xx_port_data.h"
#include "sh77xx_scif.h"

#include <utility>

namespace cerfcore {

void Sh77xxPortScifPins::OnReady() {
    ports_ = &emu_.Get<Sh77xxPortData>();
    intc_  = &emu_.Get<Sh77xxIntc>();
}

bool Sh77xxPortScifPins::TxdLow(const Sh77xxScifChannel& channel) const {
    return ports_->Drive(kSh77xxPortSc, channel.data_pin) == PinDrive::kLow;
}

bool Sh77xxPortScifPins::CtsHigh(const Sh77xxScifChannel& channel) const {
    return intc_->PinLevel(ports_->IntcPinOf(kSh77xxPortSc, channel.cts_pin));
}

void Sh77xxPortScifPins::PublishEnables(const Sh77xxScifChannel& channel, bool transmit, bool receive) {
    ports_->SetSerialEnables(kSh77xxPortSc, channel.data_pin, transmit, receive);
}

void Sh77xxPortScifPins::DriveRts(const Sh77xxScifChannel& channel, bool high) {
    ports_->SetFunctionOutputs(kSh77xxPortSc, channel.rts_pins, high ? PinDrive::kHigh : PinDrive::kLow);
}

void Sh77xxPortScifPins::AddPinListener(std::function<void()> listener) { ports_->AddDriveListener(std::move(listener)); }

}
