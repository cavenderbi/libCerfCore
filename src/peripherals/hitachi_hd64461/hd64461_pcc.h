#pragma once

#include <cstdint>

#include "../hitachi_pcc/hitachi_pcc_impl.h"

namespace cerfcore {

class Hd64461Gpio;
class Hd64461Intc;
class Hd64461SocketWiring;
class Hd64461SystemControl;

namespace cerf_hd64461_pcc_detail {

using hitachi_pcc_detail::ChangeStatus;
using hitachi_pcc_detail::Control;
using hitachi_pcc_detail::InterfaceStatus;
using hitachi_pcc_detail::RegWindowModel;

constexpr RegWindowModel Window() {
    RegWindowModel w{0x10002000u, 0x30u, 24u, false, {}};
    w.reg[0]  = InterfaceStatus();
    w.reg[1]  = Control(0xFFu, 0x00u);
    w.reg[2]  = ChangeStatus(0xBFu, 0x40u);
    w.reg[3]  = Control(0xFFu, 0x80u);
    w.reg[4]  = Control(0x03u, 0xFCu);
    w.reg[8]  = InterfaceStatus();
    w.reg[9]  = Control(0xDFu, 0x20u);
    w.reg[10] = ChangeStatus(0x8Fu, 0xF0u);
    w.reg[11] = Control(0x8Fu, 0xF0u);
    w.reg[12] = Control(0x03u, 0xFCu);
    w.reg[21] = Control(0x90u, 0x00u);
    w.reg[22] = Control(0x0Fu, 0xFFu);
    w.reg[23] = Control(0x03u, 0xFFu);
    return w;
}

inline constexpr RegWindowModel kModel = Window();

}

class Hd64461Pcc : public hitachi_pcc_detail::HitachiPccBase<cerf_hd64461_pcc_detail::kModel, true> {
public:
    using HitachiPccBase::HitachiPccBase;

    bool ShouldRegister() override;
    void OnReady() override;

    void ApplyInputGating(uint32_t gated_ports);

protected:
    ObjectIdentity       SocketIdentity(uint32_t ch) const override;
    bool                 SocketFitted(uint32_t ch) const override;
    bool                 SocketPoweredLocked(uint32_t ch) const override;
    bool                 WriteProtectedLocked(uint32_t ch) const override;
    HitachiPccSocketPins PresentLocked(uint32_t ch, const HitachiPccSocketPins& live) const override;
    void                 DeliverCausesLocked(uint32_t ch, uint32_t causes) override;
    void                 RequireCardCycle(uint32_t ch, uint32_t offset) override;

private:
    Hd64461Intc*                intc_   = nullptr;
    Hd64461Gpio*                gpio_   = nullptr;
    const Hd64461SocketWiring*  wiring_ = nullptr;
    const Hd64461SystemControl* system_ = nullptr;
};

}
