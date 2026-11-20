#pragma once

#include <cerfcore/peripherals/peripheral_base.h>
#include "../../peripherals/pin_drive.h"
#include <cerfcore/core/callback_list.h>
#include "sh77xx_intc.h"
#include "sh77xx_pfc.h"

#include <cstdint>
#include <functional>
#include <utility>

namespace cerfcore {

inline constexpr uint32_t kSh77xxPortDataBase   = 0x04000120u;
inline constexpr uint32_t kSh77xxPortDataStride = 2u;

class Sh77xxPortData : public Peripheral {
public:
    using Peripheral::Peripheral;

    virtual PinDrive Drive(uint32_t port, uint32_t pin) const = 0;
    virtual Sh77xxIntcPin IntcPinOf(uint32_t port, uint32_t pin) const;

    void AddDriveListener(std::function<void()> listener) { listeners_.Add(std::move(listener)); }

    void SetSerialEnables(uint32_t port, uint32_t pin, bool transmit, bool receive);
    void SetFunctionOutputs(uint32_t port, uint8_t pins, PinDrive drive);

protected:
    void NotifyDriveChanged() {
        listeners_.Call();
    }

    void AddFunctionListener(std::function<void()> listener) { function_listeners_.Add(std::move(listener)); }

    uint8_t Transmitting(uint32_t port) const { return transmitting_[port]; }
    uint8_t Receiving(uint32_t port) const { return receiving_[port]; }
    uint8_t FunctionOutputsLow(uint32_t port) const { return function_low_[port]; }
    uint8_t FunctionOutputsHigh(uint32_t port) const { return function_high_[port]; }

private:
    CallbackList<> listeners_;
    CallbackList<> function_listeners_;
    uint8_t        transmitting_[kSh77xxPortCount]  = {};
    uint8_t        receiving_[kSh77xxPortCount]     = {};
    uint8_t        function_low_[kSh77xxPortCount]  = {};
    uint8_t        function_high_[kSh77xxPortCount] = {};
};

}
