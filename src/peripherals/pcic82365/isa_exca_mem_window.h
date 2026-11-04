#pragma once

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/peripherals/peripheral_base.h>
#include "../peripheral_dispatcher.h"

#include <cstdint>

namespace cerfcore {

template <class Controller, class Board, uint32_t kSize>
class IsaExcaMemWindow : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        return Controller::FittedOnFamily(emu_.Get<BoardContext>().GetBoardFamilyId());
    }

    void OnReady() override {
        base_ = emu_.Get<Board>().IsaMemBase();
        emu_.Get<PeripheralDispatcher>().Register(this);
    }

    uint32_t MmioBase() const override { return base_; }
    uint32_t MmioSize() const override { return kSize; }

    uint8_t ReadByte(uint32_t addr) override {
        return emu_.Get<Controller>().ReadCardMem8(addr - base_);
    }
    uint16_t ReadHalf(uint32_t addr) override {
        return emu_.Get<Controller>().ReadCardMem16(addr - base_);
    }
    void WriteByte(uint32_t addr, uint8_t value) override {
        emu_.Get<Controller>().WriteCardMem8(addr - base_, value);
    }
    void WriteHalf(uint32_t addr, uint16_t value) override {
        emu_.Get<Controller>().WriteCardMem16(addr - base_, value);
    }

private:
    uint32_t base_ = 0u;
};

}
