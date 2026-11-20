#pragma once

#include <cstdint>

#include "../../peripherals/hitachi_pcc/hitachi_pcc_impl.h"
#include "../sh77xx/sh77xx_standby_control.h"

namespace cerfcore {

class Sh77xxIntc;
class Sh77xxPfc;
class Sh7707PccSockets;

namespace sh7707_pcc_detail {

using hitachi_pcc_detail::ChangeStatus;
using hitachi_pcc_detail::Control;
using hitachi_pcc_detail::InterfaceStatus;
using hitachi_pcc_detail::RegWindowModel;

constexpr RegWindowModel Window() {
    RegWindowModel w{0x040000E0u, 0x18u, 12u, false, {}};
    w.reg[0]  = InterfaceStatus();
    w.reg[1]  = Control(0xEFu, 0x10u);
    w.reg[2]  = ChangeStatus(0xBFu, 0x40u);
    w.reg[3]  = Control(0xFFu, 0x80u);
    w.reg[8]  = InterfaceStatus();
    w.reg[9]  = Control(0xCFu, 0x10u);
    w.reg[10] = ChangeStatus(0x8Fu, 0x70u);
    w.reg[11] = Control(0x8Fu, 0xF0u);
    return w;
}

inline constexpr RegWindowModel kModel = Window();

}

class Sh7707Pcc : public hitachi_pcc_detail::HitachiPccBase<sh7707_pcc_detail::kModel, false> {
public:
    using HitachiPccBase::HitachiPccBase;

    bool ShouldRegister() override;
    void OnReady() override;

    uint8_t  ReadByte(uint32_t addr) override;
    void     WriteByte(uint32_t addr, uint8_t value) override;
    uint16_t ReadHalf(uint32_t addr) override;
    void     WriteHalf(uint32_t addr, uint16_t value) override;

    void SetSocketPower(uint32_t channel, bool on);

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;
    void PostRestore() override;

protected:
    ObjectIdentity       SocketIdentity(uint32_t ch) const override;
    bool                 SocketFitted(uint32_t ch) const override;
    bool                 SocketPoweredLocked(uint32_t ch) const override;
    bool                 WriteProtectedLocked(uint32_t ch) const override;
    HitachiPccSocketPins PresentLocked(uint32_t ch, const HitachiPccSocketPins& live) const override;
    void                 DeliverCausesLocked(uint32_t ch, uint32_t causes) override;
    void                 RequireCardCycle(uint32_t ch, uint32_t offset) override;
    uint16_t             SampleRegister(uint32_t i, uint32_t addr) override;
    void                 BeforeStore(uint32_t i, uint16_t value) override;

private:
    void    RequireClock(const char* access, uint32_t addr, uint32_t value) const;
    void    TrackPinFunctions();
    void    ReadPinFunctions(uint8_t (&inputs)[2], uint8_t& outputs) const;
    uint8_t RoutedPins(uint32_t port) const;

    Sh77xxIntc*             intc_    = nullptr;
    Sh77xxPfc*              pfc_     = nullptr;
    Sh77xxStbcr2*           stbcr2_  = nullptr;
    const Sh7707PccSockets* sockets_ = nullptr;
    bool                    power_[2]          = {};
    uint8_t                 routed_inputs_[2]  = {};
    uint8_t                 routed_outputs_    = 0u;
    HitachiPccSocketPins    held_[2]           = {};
};

}
