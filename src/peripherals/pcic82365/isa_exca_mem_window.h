#pragma once

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/peripherals/peripheral_base.h>
#include "../../core/board_parts.h"
#include "../peripheral_dispatcher.h"
#include "isa_host_bus_rule.h"

#include <cstdint>

namespace cerfcore {

template <class Controller, uint32_t kSize, class Host = UnrestrictedIsaHost>
class IsaExcaMemWindow : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        return emu_.Get<BoardParts>().Has(Controller::kPartId);
    }

    void OnReady() override {
        pcic_ = &emu_.Get<Controller>();
        base_ = AreaBase();
        emu_.Get<PeripheralDispatcher>().Register(this);
    }

    uint32_t MmioBase() const override { return base_; }
    uint32_t MmioSize() const override { return kSize; }

    uint8_t ReadByte(uint32_t addr) override {
        if constexpr (Host::kRestricted) return pcic_->ReadCardMem8(host_, addr - base_);
        else return pcic_->ReadCardMem8(addr - base_);
    }
    uint16_t ReadHalf(uint32_t addr) override {
        if constexpr (Host::kRestricted) return pcic_->ReadCardMem16(host_, addr - base_);
        else return pcic_->ReadCardMem16(addr - base_);
    }
    void WriteByte(uint32_t addr, uint8_t value) override {
        if constexpr (Host::kRestricted) pcic_->WriteCardMem8(host_, addr - base_, value);
        else pcic_->WriteCardMem8(addr - base_, value);
    }
    void WriteHalf(uint32_t addr, uint16_t value) override {
        if constexpr (Host::kRestricted) pcic_->WriteCardMem16(host_, addr - base_, value);
        else pcic_->WriteCardMem16(addr - base_, value);
    }
    uint32_t ReadWord  (uint32_t addr) override { return ReadWordAsHalves(addr); }
    void     WriteWord (uint32_t addr, uint32_t value) override { WriteWordAsHalves(addr, value); }
    uint64_t ReadDword (uint32_t addr) override { return ReadDwordAsHalves(addr); }
    void     WriteDword(uint32_t addr, uint64_t value) override { WriteDwordAsHalves(addr, value); }

protected:
    virtual uint32_t AreaBase() = 0;

private:
    Host        host_{};
    Controller* pcic_ = nullptr;
    uint32_t    base_ = 0u;
};

}
