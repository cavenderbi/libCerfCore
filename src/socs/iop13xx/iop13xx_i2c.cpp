#include "iop13xx_i2c_device.h"

#include "../intel_i2c_unit.h"
#include <cerfcore/boards/board_context.h>
#include "iop13xx_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/peripherals/peripheral_base.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include "../irq_controller.h"
#include <cerfcore/state/state_stream.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include "iop13xx_cp6.h"

#include <cstdint>

namespace cerfcore {

namespace {

/* Intel 81341 and 81342 I/O Processors Developer's Manual, chapter 14 and
   Peripheral Register Summary: I2C0 occupies PMMR +0x2500 and uses the
   ICR/ISR MSTART/TBYTE interrupt-driven transfer contract. */
constexpr uint32_t kI2cBase = 0xFFD82500u;
constexpr uint32_t kI2cSize = 0x0000001Cu;
constexpr int kI2cIrqSource = 10;
constexpr uint32_t kI2cIrqContributor = 1u << 0;
constexpr uint32_t kCrOffset = 0x00u;
constexpr uint32_t kSrOffset = 0x04u;
constexpr uint32_t kSarOffset = 0x08u;
constexpr uint32_t kDbrOffset = 0x0Cu;
constexpr uint32_t kBmrOffset = 0x14u;
constexpr uint32_t kMbcrOffset = 0x18u;

constexpr uint32_t kMbcrKnownMask = 0x0007u;

class Iop13xxI2c final : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        auto* board = emu_.TryGet<BoardContext>();
        return board && board->GetSocId() == SocId::Iop13xx;
    }

    void OnReady() override {
        ResetUnit();
        emu_.Get<PeripheralDispatcher>().Register(this);
        emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) {
            if (emu_.Get<GuestCpuReset>().DeliveredResetWasResume()) return;
            ResetUnit();
        });
    }

    void OnShutdown() override {
        if (auto* device = emu_.TryGet<Iop13xxI2cDevice>()) device->BusStop();
    }

    uint32_t MmioBase() const override { return kI2cBase; }
    uint32_t MmioSize() const override { return kI2cSize; }

    uint32_t ReadWord(uint32_t addr) override { return ReadWordRaw(addr & ~3u); }

    uint16_t ReadHalf(uint32_t addr) override {
        const uint32_t word = ReadWordRaw(addr & ~3u);
        return static_cast<uint16_t>(word >> ((addr & 2u) * 8u));
    }

    uint8_t ReadByte(uint32_t addr) override {
        const uint32_t word = ReadWordRaw(addr & ~3u);
        return static_cast<uint8_t>(word >> ((addr & 3u) * 8u));
    }

    void WriteWord(uint32_t addr, uint32_t value) override { WriteWordRaw(addr & ~3u, value); }

    void WriteHalf(uint32_t addr, uint16_t value) override {
        const uint32_t aligned = addr & ~3u;
        const uint32_t shift = (addr & 2u) * 8u;
        const uint32_t mask = 0xFFFFu << shift;
        WriteWordRaw(aligned, (ReadWordRaw(aligned) & ~mask) | (static_cast<uint32_t>(value) << shift));
    }

    void WriteByte(uint32_t addr, uint8_t value) override {
        const uint32_t aligned = addr & ~3u;
        const uint32_t shift = (addr & 3u) * 8u;
        const uint32_t mask = 0xFFu << shift;
        WriteWordRaw(aligned, (ReadWordRaw(aligned) & ~mask) | (static_cast<uint32_t>(value) << shift));
    }

    void SaveState(StateWriter& writer) override {
        unit_.SaveState(writer);
        writer.Write("imbcr", imbcr_);
        writer.Write("i2c_irq_line_asserted", i2c_irq_line_asserted_);
        auto* device = emu_.TryGet<Iop13xxI2cDevice>();
        const bool has_device = device != nullptr;
        writer.Write("has_device", has_device);
        if (device) device->SaveState(writer);
    }

    void RestoreState(StateReader& reader) override {
        unit_.RestoreState(reader);
        reader.Read("imbcr", imbcr_);
        reader.Read("i2c_irq_line_asserted", i2c_irq_line_asserted_);
        bool has_device = false;
        reader.Read("has_device", has_device);
        auto* device = emu_.TryGet<Iop13xxI2cDevice>();
        if (has_device != (device != nullptr))
            reader.Reject(has_device ? "the image has an I2C device this build does not have"
                                     : "this build has an I2C device the image does not have");
        if (device) device->RestoreState(reader);
    }

    void PostRestore() override {
        if (auto* device = emu_.TryGet<Iop13xxI2cDevice>()) device->PostRestore();
    }

private:
    I2cSlave* Target() { return emu_.TryGet<Iop13xxI2cDevice>(); }

    uint32_t ReadWordRaw(uint32_t addr) {
        switch (addr - kI2cBase) {
        case kCrOffset: return unit_.Icr();
        case kSrOffset: return unit_.Isr();
        case kSarOffset: return unit_.Isar();
        case kDbrOffset: return unit_.Idbr();
        case kBmrOffset: return IntelI2cUnit::kIbmrIdle;
        case kMbcrOffset: return imbcr_;
        default: HaltUnsupportedAccess("IOP13xx I2C register read", addr, 0);
        }
    }

    void WriteWordRaw(uint32_t addr, uint32_t value) {
        switch (addr - kI2cBase) {
        case kCrOffset: unit_.WriteIcr(value, Target()); RefreshInterrupt(); return;
        case kSrOffset: unit_.WriteIsr(value); RefreshInterrupt(); return;
        case kSarOffset: unit_.WriteIsar(value); return;
        case kDbrOffset: unit_.WriteIdbr(value); return;
        case kMbcrOffset: imbcr_ = value & kMbcrKnownMask; return;
        default: HaltUnsupportedAccess("IOP13xx I2C register write", addr, value);
        }
    }

    void ResetUnit() {
        unit_.Reset(Target());
        unit_.WriteIsar(0u);
        imbcr_ = 0;
        RefreshInterrupt();
    }

    void RefreshInterrupt() {
        auto& irq = emu_.Get<IrqController>();
        if (unit_.InterruptActive()) {
            if (!i2c_irq_line_asserted_) {
                i2c_irq_line_asserted_ = true;
                /* Intel 81341/81342 Developer's Manual, Figure 115 and
                   IINTSRC0 bit 10: I2C0 drives interrupt-controller source 10. */
                irq.SetSharedIrqLevel(kI2cIrqSource, kI2cIrqContributor, true);
            }
        } else if (i2c_irq_line_asserted_) {
            i2c_irq_line_asserted_ = false;
            irq.SetSharedIrqLevel(kI2cIrqSource, kI2cIrqContributor, false);
        }
    }

    IntelI2cUnit unit_;
    uint32_t     imbcr_ = 0;
    bool         i2c_irq_line_asserted_ = false;
};

REGISTER_SERVICE(Iop13xxI2c);

} // namespace

}
