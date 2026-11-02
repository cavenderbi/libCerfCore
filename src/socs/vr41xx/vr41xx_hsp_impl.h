#pragma once

#include <cerfcore/peripherals/peripheral_base.h>

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include <cerfcore/state/state_stream.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include "vr41xx_cmu.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace cerfcore {

namespace cerf_vr41xx_hsp_detail {

struct Vr41xxHspChip {
    uint8_t signature_confirmation;
    bool    hsprst_resets_init;
};

inline constexpr uint32_t kHspBase          = 0x0C000020u;
inline constexpr uint32_t kHspSize          = 0x0Au;
inline constexpr uint32_t kOffInit          = 0x00u;
inline constexpr uint32_t kOffData          = 0x02u;
inline constexpr uint32_t kOffIndex         = 0x04u;
inline constexpr uint32_t kOffSignature     = 0x09u;
inline constexpr uint16_t kInitWritable     = 0x001Fu;
inline constexpr uint16_t kInitHsprst       = 0x0001u;
inline constexpr uint16_t kIndexExtOut      = 2u;
inline constexpr uint16_t kExtOutWritable   = 0x006Du;
inline constexpr uint16_t kExtOutAfterReset = 0x0004u;
inline constexpr std::array<uint8_t, 6> kSignature = { 'P', 'C', 't', 'e', 'l', 0xAAu };

template <const std::string_view& Soc, const Vr41xxHspChip& Chip>
class Vr41xxHspBase : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetSocId() == Soc;
    }

    void OnReady() override {
        emu_.Get<PeripheralDispatcher>().Register(this);
        emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) {
            init_ = 0u;
            ResetCore();
        });
    }

    uint32_t MmioBase() const override { return kHspBase; }
    uint32_t MmioSize() const override { return kHspSize; }

    uint16_t ReadHalf(uint32_t addr) override {
        if (addr - kHspBase == kOffInit)
            return init_;
        HaltUnsupportedAccess("HSP ReadHalf", addr, 0);
    }

    uint8_t ReadByte(uint32_t addr) override {
        switch (addr - kHspBase) {
            case kOffInit:
                return static_cast<uint8_t>(init_);
            case kOffSignature:
                RequireCoreAccess("HSPPCS read", std::nullopt);
                return Chip.signature_confirmation;
            default:
                HaltUnsupportedAccess("HSP ReadByte", addr, 0);
        }
    }

    void WriteHalf(uint32_t addr, uint16_t value) override {
        switch (addr - kHspBase) {
            case kOffInit:
                WriteInit(value);
                return;
            case kOffIndex:
                if (CoreAccepts("HSPINDEX write", value))
                    index_ = value;
                return;
            case kOffData:
                if (CoreAccepts("HSPDATA write", value))
                    WriteData(value);
                return;
            default:
                HaltUnsupportedAccess("HSP WriteHalf", addr, value);
        }
    }

    void WriteByte(uint32_t addr, uint8_t value) override {
        if (addr - kHspBase == kOffSignature) {
            WriteSignature(value);
            return;
        }
        HaltUnsupportedAccess("HSP WriteByte", addr, value);
    }

    uint32_t ReadWord(uint32_t addr) override { HaltUnsupportedAccess("HSP ReadWord", addr, 0); }
    void WriteWord(uint32_t addr, uint32_t value) override { HaltUnsupportedAccess("HSP WriteWord", addr, value); }

    void SaveState(StateWriter& w) override {
        w.Write("init", init_);
        w.Write<uint8_t>("index_written", index_ ? 1u : 0u);
        w.Write<uint16_t>("index", index_.value_or(0u));
        w.Write("extout", extout_);
        w.Write<uint8_t>("unlocked", unlocked_ ? 1u : 0u);
        w.Write("signature_pos", signature_pos_);
        w.Write<uint8_t>("signature_broken", signature_broken_ ? 1u : 0u);
    }

    void RestoreState(StateReader& r) override {
        r.Read("init", init_);
        uint8_t index_written = 0u;
        r.Read("index_written", index_written);
        uint16_t index = 0u;
        r.Read("index", index);
        index_.reset();
        if (index_written != 0u)
            index_ = index;
        r.Read("extout", extout_);
        uint8_t unlocked = 0u;
        r.Read("unlocked", unlocked);
        unlocked_ = unlocked != 0u;
        r.Read("signature_pos", signature_pos_);
        uint8_t broken = 0u;
        r.Read("signature_broken", broken);
        signature_broken_ = broken != 0u;
    }

private:
    void RequireClock(const char* operation) const {
        emu_.Get<Vr41xxCmu>().RequireClock(kCmuMskShsp, operation);
    }

    bool CoreHeldInReset() const { return (init_ & kInitHsprst) != 0u; }

    void ResetCore() {
        index_.reset();
        extout_ = kExtOutAfterReset;
        unlocked_ = false;
        signature_pos_ = 0u;
        signature_broken_ = false;
    }

    [[noreturn]] void DieCore(const char* access, std::optional<uint16_t> value, const char* reason) const {
        if (value)
            emu_.Get<Fatal>().Die("[HSP] %s 0x%04X %s", access, *value, reason);
        emu_.Get<Fatal>().Die("[HSP] %s %s", access, reason);
    }

    void RequireCoreOutOfReset(const char* access, std::optional<uint16_t> value) const {
        RequireClock(access);
        if (CoreHeldInReset())
            DieCore(access, value, "while HSPRST holds the modem core in reset");
    }

    void RequireNoPartialSignature(const char* access, std::optional<uint16_t> value) const {
        if (signature_broken_ || signature_pos_ != 0u)
            DieCore(access, value, "during an incomplete or broken HSPPCTEL sequence");
    }

    void RequireCoreAccess(const char* access, std::optional<uint16_t> value) const {
        RequireCoreOutOfReset(access, value);
        RequireNoPartialSignature(access, value);
        if (!unlocked_)
            DieCore(access, value, "before the HSPPCTEL signature");
    }

    bool CoreAccepts(const char* access, uint16_t value) const {
        if (!unlocked_ && !signature_broken_ && signature_pos_ == 0u && !CoreHeldInReset())
            return false;
        RequireCoreAccess(access, value);
        return true;
    }

    void WriteInit(uint16_t value) {
        if ((value & ~kInitWritable) != 0u)
            emu_.Get<Fatal>().Die("[HSP] HSPINIT write 0x%04X sets reserved bits", value);
        if ((value & kInitHsprst) != 0u) {
            ResetCore();
            init_ = Chip.hsprst_resets_init ? 0u : value;
            return;
        }
        init_ = value;
    }

    void WriteData(uint16_t value) {
        if (!index_)
            emu_.Get<Fatal>().Die("[HSP] HSPDATA write 0x%04X with no HSPINDEX write since reset", value);
        if (*index_ != kIndexExtOut)
            emu_.Get<Fatal>().Die("[HSP] HSPDATA write 0x%04X to index %u, which the model does not cover",
                                  value, *index_);
        if ((value & ~kExtOutWritable) != 0u)
            emu_.Get<Fatal>().Die("[HSP] HSPEXTOUT write 0x%04X sets reserved bits", value);
        extout_ = value;
    }

    void WriteSignature(uint8_t value) {
        RequireCoreOutOfReset("HSPPCTEL write", value);
        if (signature_broken_)
            emu_.Get<Fatal>().Die("[HSP] HSPPCTEL write 0x%02X after a broken signature sequence", value);
        if (value == kSignature[signature_pos_]) {
            ++signature_pos_;
            if (signature_pos_ == kSignature.size()) {
                unlocked_ = true;
                signature_pos_ = 0u;
            }
            return;
        }
        if (signature_pos_ == 1u && value == kSignature[0]) {
            signature_broken_ = true;
            return;
        }
        emu_.Get<Fatal>().Die("[HSP] HSPPCTEL write 0x%02X outside the PCtel signature sequence", value);
    }

    uint16_t init_          = 0u;
    std::optional<uint16_t> index_;
    uint16_t extout_        = kExtOutAfterReset;
    bool     unlocked_      = false;
    uint8_t  signature_pos_ = 0u;
    bool     signature_broken_ = false;
};

}

}
