#include "pci1225_interrupt_terminals.h"

#include "pci1225_config_space.h"

#include <array>
#include <cstddef>

namespace cerfcore {

namespace {

constexpr uint8_t  kPinInta     = 0x01u;
constexpr uint8_t  kPinIntb     = 0x02u;
constexpr uint8_t  kSerialSlots = 4u;

constexpr uint32_t kSysIntrTie      = 0x20000000u;
constexpr unsigned kSysSerStepShift = 30u;

constexpr uint32_t kMfunc0       = 0x0000000Fu;
constexpr uint32_t kMfunc0Inta   = 0x00000002u;
constexpr uint32_t kMfunc1       = 0x000000F0u;
constexpr uint32_t kMfunc1Intb   = 0x00000020u;
constexpr uint32_t kMfunc3       = 0x0000F000u;
constexpr uint32_t kMfunc3Irqser = 0x00001000u;
constexpr unsigned kMfuncFieldBits = 4u;
constexpr uint32_t kMfuncField   = 0xFu;
constexpr uint8_t  kNoIsaIrq     = 0u;

constexpr std::array<std::array<uint8_t, 16>, 7> kIsaIrqOfMfuncCode = {{
    {{ 0, 0, 0, 3, 4, 5, 0, 0, 0, 9, 10, 11, 0, 0, 0, 15 }},
    {{ 0, 0, 0, 3, 4, 5, 0, 0, 0, 9, 10, 11, 0, 0, 0, 15 }},
    {{ 0, 0, 0, 3, 4, 5, 0, 0, 0, 9, 10, 11, 0, 0, 0, 7 }},
    {{ 0, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }},
    {{ 0, 0, 0, 3, 4, 5, 0, 0, 0, 9, 10, 11, 0, 0, 0, 15 }},
    {{ 0, 0, 0, 3, 4, 5, 0, 0, 0, 9, 10, 11, 0, 0, 0, 15 }},
    {{ 0, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }},
}};

constexpr uint8_t kDcIntMode            = 0x06u;
constexpr uint8_t kDcIntModeParallelPci = 0x00u;
constexpr uint8_t kDcIntModeParallelIrq = 0x02u;
constexpr uint8_t kDcIntModeIrqSerial   = 0x04u;
constexpr uint8_t kDcIntModeAllSerial   = 0x06u;

constexpr size_t   kMfunc0Terminal     = 0u;
constexpr size_t   kMfunc1Terminal     = 1u;
constexpr size_t   kMfunc3Terminal     = 3u;
constexpr uint32_t kMfuncResetCode     = 0x0u;
constexpr uint32_t kMfuncIrqserCode    = 0x1u;
constexpr uint32_t kMfuncPciIntCode    = 0x2u;

bool TerminalCodeModeled(size_t terminal, uint32_t code) {
    if (code == kMfuncResetCode || kIsaIrqOfMfuncCode[terminal][code] != kNoIsaIrq)
        return true;
    if (terminal == kMfunc0Terminal || terminal == kMfunc1Terminal)
        return code == kMfuncPciIntCode;
    return terminal == kMfunc3Terminal && code == kMfuncIrqserCode;
}

constexpr size_t kSerialBusDataTerminal  = 1u;
constexpr size_t kSerialBusClockTerminal = 4u;

bool SerialBusTerminal(size_t terminal) {
    return terminal == kSerialBusDataTerminal || terminal == kSerialBusClockTerminal;
}

uint16_t ParallelIsaIrqTerminals(uint32_t mfunc, bool serial_bus) {
    uint16_t irqs = 0u;
    for (size_t terminal = 0; terminal < kIsaIrqOfMfuncCode.size(); ++terminal) {
        if (serial_bus && SerialBusTerminal(terminal))
            continue;
        const uint32_t code = (mfunc >> (terminal * kMfuncFieldBits)) & kMfuncField;
        const uint8_t irq = kIsaIrqOfMfuncCode[terminal][code];
        if (irq != kNoIsaIrq)
            irqs = static_cast<uint16_t>(irqs | (1u << irq));
    }
    return irqs;
}

}

bool Pci1225InterruptTerminals::RoutingModeled(uint32_t mfunc) {
    for (size_t terminal = 0; terminal < kIsaIrqOfMfuncCode.size(); ++terminal) {
        if (!TerminalCodeModeled(terminal, (mfunc >> (terminal * kMfuncFieldBits)) & kMfuncField))
            return false;
    }
    return true;
}

uint16_t Pci1225InterruptTerminals::IsaIrqBit(uint8_t select) {
    return select == kNoIsaIrq ? 0u : static_cast<uint16_t>(1u << select);
}

bool Pci1225InterruptTerminals::PciInterruptsSerialized() const {
    return (shared_.device_control & kDcIntMode) == kDcIntModeAllSerial;
}

bool Pci1225InterruptTerminals::IsaIrqsSerialized() const {
    const uint8_t mode = shared_.device_control & kDcIntMode;
    return mode == kDcIntModeIrqSerial || mode == kDcIntModeAllSerial;
}

uint8_t Pci1225InterruptTerminals::InterruptPin() const {
    return function_ == 0u || (shared_.system_global & kSysIntrTie) != 0u ? kPinInta : kPinIntb;
}

uint8_t Pci1225InterruptTerminals::SignaledSlot() const {
    if (!PciInterruptsSerialized()) return InterruptPin();
    const uint32_t step = shared_.system_global >> kSysSerStepShift;
    return static_cast<uint8_t>(((InterruptPin() - 1u + step) % kSerialSlots) + 1u);
}

bool Pci1225InterruptTerminals::PciInterruptSignaled() const {
    if (PciInterruptsSerialized())
        return (shared_.mfunc & kMfunc3) == kMfunc3Irqser;
    if (InterruptPin() == kPinInta)
        return (shared_.mfunc & kMfunc0) == kMfunc0Inta;
    return !shared_.SerialBusDetected() && (shared_.mfunc & kMfunc1) == kMfunc1Intb;
}

uint16_t Pci1225InterruptTerminals::SignaledIsaIrqs(uint16_t requested) const {
    const uint8_t mode = shared_.device_control & kDcIntMode;
    if (mode == kDcIntModeParallelPci)
        return 0u;
    if (mode == kDcIntModeParallelIrq)
        return static_cast<uint16_t>(requested & DrivenParallelIsaIrqs());
    return (shared_.mfunc & kMfunc3) == kMfunc3Irqser ? requested : 0u;
}

uint16_t Pci1225InterruptTerminals::DrivenParallelIsaIrqs() const {
    if ((shared_.device_control & kDcIntMode) != kDcIntModeParallelIrq)
        return 0u;
    return ParallelIsaIrqTerminals(shared_.mfunc, shared_.SerialBusDetected());
}

}
