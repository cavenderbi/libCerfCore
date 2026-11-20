#include <cerfcore/peripherals/peripheral_base.h>

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include "casio_cassiopeia_a10_family_id.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kStatus       = 0x100000C2u;
constexpr uint32_t kKey1         = 0x100000C4u;
constexpr uint32_t kKey2         = 0x100000C6u;
constexpr uint16_t kKey1Value    = 0x5467u;
constexpr uint16_t kKey2Value    = 0x6946u;
constexpr uint16_t kKey1RelockA  = 0x0007u;
constexpr uint16_t kKey1RelockB  = 0x0006u;
constexpr uint16_t kKey1Clear    = 0x0000u;
constexpr uint16_t kKey2RelockA  = 0x0094u;
constexpr uint16_t kKey2RelockB  = 0x0014u;
constexpr uint16_t kUnlockedBits = 0x0006u;
constexpr uint16_t kStatusStored = 0x0001u;

class CasioCassiopeiaA10AsicLock : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10;
    }

    void OnReady() override {
        emu_.Get<PeripheralDispatcher>().Register(this);
        emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) { Reset(); });
    }

    uint32_t MmioBase() const override { return kStatus; }
    uint32_t MmioSize() const override { return kKey2 + 2u - kStatus; }

    uint16_t ReadHalf(uint32_t addr) override {
        if (addr != kStatus || !status_written_ || lock_unobserved_) HaltUnsupportedAccess("ReadHalf", addr, 0u);
        return static_cast<uint16_t>(status_ | (Unlocked() ? kUnlockedBits : 0u));
    }

    void WriteHalf(uint32_t addr, uint16_t value) override {
        switch (addr) {
        case kStatus:
            if (!Unlocked() || (value & ~kStatusStored) != 0u) HaltUnsupportedAccess("WriteHalf", addr, value);
            status_         = value;
            status_written_ = true;
            return;
        case kKey1:
            if (value != kKey1Value && value != kKey1RelockA && value != kKey1RelockB && value != kKey1Clear) {
                HaltUnsupportedAccess("WriteHalf", addr, value);
            }
            if (value == kKey1Clear) lock_unobserved_ = true;
            key1_ = value;
            if (Unlocked()) lock_unobserved_ = false;
            return;
        case kKey2:
            if (value != kKey2Value && value != kKey2RelockA && value != kKey2RelockB) {
                HaltUnsupportedAccess("WriteHalf", addr, value);
            }
            if (value != kKey2Value) lock_unobserved_ = true;
            key2_ = value;
            if (Unlocked()) lock_unobserved_ = false;
            return;
        default:
            HaltUnsupportedAccess("WriteHalf", addr, value);
        }
    }

    void SaveState(StateWriter& w) override {
        w.Write("status", status_);
        w.Write("status_written", status_written_);
        w.Write("key1", key1_);
        w.Write("key2", key2_);
        w.Write("lock_unobserved", lock_unobserved_);
    }

    void RestoreState(StateReader& r) override {
        r.Read("status", status_);
        r.Read("status_written", status_written_);
        r.Read("key1", key1_);
        r.Read("key2", key2_);
        r.Read("lock_unobserved", lock_unobserved_);
    }

private:
    bool Unlocked() const { return key1_ == kKey1Value && key2_ == kKey2Value; }

    void Reset() {
        status_         = 0u;
        status_written_ = false;
        key1_           = 0u;
        key2_           = 0u;
        lock_unobserved_ = false;
    }

    uint16_t status_          = 0u;
    bool     status_written_  = false;
    uint16_t key1_            = 0u;
    uint16_t key2_            = 0u;
    bool     lock_unobserved_ = false;
};

}

REGISTER_SERVICE(CasioCassiopeiaA10AsicLock);

}
