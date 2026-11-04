#include "nec_vrc4171a.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/peripherals/peripheral_base.h>
#include "../peripheral_dispatcher.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kSize          = 0x01010000u;
constexpr uint32_t kBlockShift    = 16u;
constexpr uint32_t kPortMask      = 0xFFFFu;
constexpr uint32_t kCardIoBlock   = 0x100u;
constexpr uint32_t kPortIndex     = 0x3E0u;
constexpr uint32_t kPortData      = 0x3E1u;

class NecVrc4171aIoWindow : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        return NecVrc4171a::FittedOnFamily(emu_.Get<BoardContext>().GetBoardFamilyId());
    }

    void OnReady() override {
        base_ = emu_.Get<NecVrc4171aBoard>().IsaIoBase();
        emu_.Get<PeripheralDispatcher>().Register(this);
    }

    uint32_t MmioBase() const override { return base_; }
    uint32_t MmioSize() const override { return kSize; }

    void SaveState(StateWriter& w) override { emu_.Get<NecVrc4171a>().SaveState(w); }
    void RestoreState(StateReader& r) override { emu_.Get<NecVrc4171a>().RestoreState(r); }
    void PostRestore() override { emu_.Get<NecVrc4171a>().PostRestore(); }

    uint8_t ReadByte(uint32_t addr) override {
        const uint32_t off = addr - base_;
        const uint32_t port = off & kPortMask;
        auto& pcic = emu_.Get<NecVrc4171a>();
        if (RegistersDecode(off)) {
            if (port == kPortIndex || port == kPortData) return pcic.ReadPcicByte(port - kPortIndex);
            if (NecVrc4171a::IsTopLevelPort(port)) HaltUnsupportedAccess("VRC4171A top-level byte read", addr, 0);
        }
        if (CardIoBlock(off)) return pcic.ReadCardIo8(port);
        HaltUnsupportedAccess("VRC4171A ISA I/O byte read", addr, 0);
    }

    uint16_t ReadHalf(uint32_t addr) override {
        const uint32_t off = addr - base_;
        const uint32_t port = off & kPortMask;
        auto& pcic = emu_.Get<NecVrc4171a>();
        if (RegistersDecode(off)) {
            if (NecVrc4171a::IsTopLevelPort(port)) return pcic.ReadTopLevel(port);
            if (port == kPortIndex) HaltUnsupportedAccess("VRC4171A 16-bit index/data read", addr, 0);
        }
        if (CardIoBlock(off)) return pcic.ReadCardIo16(port);
        HaltUnsupportedAccess("VRC4171A ISA I/O halfword read", addr, 0);
    }

    void WriteByte(uint32_t addr, uint8_t value) override {
        const uint32_t off = addr - base_;
        const uint32_t port = off & kPortMask;
        auto& pcic = emu_.Get<NecVrc4171a>();
        if (RegistersDecode(off)) {
            if (port == kPortIndex || port == kPortData) {
                pcic.WritePcicByte(port - kPortIndex, value);
                return;
            }
            if (NecVrc4171a::IsTopLevelPort(port)) HaltUnsupportedAccess("VRC4171A top-level byte write", addr, value);
        }
        if (CardIoBlock(off)) {
            pcic.WriteCardIo8(port, value);
            return;
        }
        HaltUnsupportedAccess("VRC4171A ISA I/O byte write", addr, value);
    }

    void WriteHalf(uint32_t addr, uint16_t value) override {
        const uint32_t off = addr - base_;
        const uint32_t port = off & kPortMask;
        auto& pcic = emu_.Get<NecVrc4171a>();
        if (RegistersDecode(off)) {
            if (NecVrc4171a::IsTopLevelPort(port)) {
                pcic.WriteTopLevel(port, value);
                return;
            }
            if (port == kPortIndex) HaltUnsupportedAccess("VRC4171A 16-bit index/data write", addr, value);
        }
        if (CardIoBlock(off)) {
            pcic.WriteCardIo16(port, value);
            return;
        }
        HaltUnsupportedAccess("VRC4171A ISA I/O halfword write", addr, value);
    }

private:
    bool RegistersDecode(uint32_t off) {
        return !emu_.Get<NecVrc4171a>().IoUpperDecode() || CardIoBlock(off);
    }
    static bool CardIoBlock(uint32_t off) { return (off >> kBlockShift) == kCardIoBlock; }

    uint32_t base_ = 0u;
};

}

REGISTER_SERVICE(NecVrc4171aIoWindow);

}
