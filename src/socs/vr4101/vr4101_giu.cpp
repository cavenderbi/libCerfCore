#include "../vr41xx/vr41xx_cmu.h"
#include "../vr41xx/vr41xx_giu.h"
#include "../vr41xx/vr41xx_giu_input_pins.h"
#include "../vr41xx/vr41xx_giu_interrupts.h"
#include "../vr41xx/vr41xx_giu_regs.h"
#include "../vr41xx/vr41xx_icu.h"
#include "../vr41xx/vr41xx_pmu.h"
#include "vr4101_id.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/host/guest_deep_sleep.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../../peripherals/peripheral_dispatcher.h"

#include <cstdint>
#include <mutex>

namespace cerfcore {

namespace {

using namespace cerf_vr41xx_giu_detail;

constexpr uint32_t kBase       = 0x0B000100u;
constexpr uint32_t kOffGouten  = 0x00u;
constexpr uint32_t kOffGpotdat = 0x02u;
constexpr uint32_t kOffGintst  = 0x04u;
constexpr uint32_t kOffGinten  = 0x06u;
constexpr uint32_t kOffGcints  = 0x08u;
constexpr uint32_t kOffGlints  = 0x0Au;

constexpr uint16_t kGpioBits       = 0x0FFFu;
constexpr uint16_t kPinBits        = 0x2FFFu;
constexpr uint16_t kMasterOutPins  = 0x2600u;
constexpr int      kDcdBit         = 13;
constexpr uint16_t kCmuMskGiu      = 1u << 4;

class Vr4101Giu : public Vr41xxGiu {
public:
    using Vr41xxGiu::Vr41xxGiu;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Vr4101; }

    void OnReady() override {
        pmu_ = &emu_.Get<Vr41xxPmu>();
        icu_ = &emu_.Get<Vr41xxIcu>();
        cmu_ = &emu_.Get<Vr41xxCmu>();
        emu_.Get<PeripheralDispatcher>().Register(this);
        emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) { ResetUnit(); });
        emu_.Get<GuestDeepSleep>().RegisterSleepEntryListener([this] { ResetUnit(); });
        cmu_->RegisterClockListener(kCmuMskGiu, [this](bool supplied) {
            std::lock_guard<std::mutex> lk(mtx_);
            EvaluateLevelsLocked(supplied);
        });
    }

    uint32_t MmioBase() const override { return kBase; }
    uint32_t MmioSize() const override { return 0x20u; }

    uint16_t ReadHalf(uint32_t addr) override {
        cmu_->RequireClock(kCmuMskGiu, "VR4101 GIU register read");
        std::lock_guard<std::mutex> lk(mtx_);
        const uint32_t off = addr - kBase;
        switch (off) {
            case kOffGouten:  return gouten_;
            case kOffGpotdat: return static_cast<uint16_t>(EffectivePinsLocked() & kPinBits);
            default: {
                uint16_t value = 0u;
                if (!irq_.ReadRegister(HelperOffset(off), &value)) HaltUnsupportedAccess("ReadHalf", addr, 0);
                return value;
            }
        }
    }

    void WriteHalf(uint32_t addr, uint16_t value) override {
        cmu_->RequireClock(kCmuMskGiu, "VR4101 GIU register write");
        const uint32_t off = addr - kBase;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            switch (off) {
                case kOffGouten:
                    if ((value & ~kGpioBits) != 0u) HaltUnsupportedAccess("WriteHalf", addr, value);
                    gouten_ = value;
                    break;
                case kOffGpotdat:
                    if ((value & ~kPinBits) != 0u) HaltUnsupportedAccess("WriteHalf", addr, value);
                    out_ = static_cast<uint16_t>(value & kGpioBits);
                    break;
                default:
                    if ((value & ~kPinBits) != 0u || !irq_.WriteRegister(HelperOffset(off), value)) {
                        HaltUnsupportedAccess("WriteHalf", addr, value);
                    }
                    break;
            }
            RequireInputInterruptsLocked();
            EvaluateLevelsLocked(TclockLocked());
        }
        if (off == kOffGouten || off == kOffGpotdat) NotifyOutputDriveChanged();
    }

    void SetPinLevel(int pin, bool level) override {
        const int bit = RegisterBit(pin);
        std::lock_guard<std::mutex> lk(mtx_);
        ApplyPinLevelLocked(pin, bit, inputs_.Drive(bit, level));
    }

    void ReleasePin(int pin) override {
        const int bit = RegisterBit(pin);
        std::lock_guard<std::mutex> lk(mtx_);
        if (const auto edge = inputs_.Release(bit)) ApplyPinLevelLocked(pin, bit, *edge);
    }

    bool GetPinLevel(int pin) override {
        const int bit = RegisterBit(pin);
        std::lock_guard<std::mutex> lk(mtx_);
        return ((EffectivePinsLocked() >> bit) & 1u) != 0u;
    }

    PinDrive OutputDrive(int gpio) override {
        if (const auto drive = AlternateDrive(gpio)) return *drive;
        const int bit = RegisterBit(gpio);
        std::lock_guard<std::mutex> lk(mtx_);
        if (((gouten_ >> bit) & 1u) == 0u) return PinDrive::kUndriven;
        return ((out_ >> bit) & 1u) != 0u ? PinDrive::kHigh : PinDrive::kLow;
    }

    void SaveState(StateWriter& w) override {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            w.Write("gouten", gouten_);
            w.Write("gpotdat_out", out_);
            irq_.Save(w);
            inputs_.Save(w);
        }
        SavePinDevices(w);
    }

    void RestoreState(StateReader& r) override {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            r.Read("gouten", gouten_);
            r.Read("gpotdat_out", out_);
            irq_.Restore(r);
            inputs_.Restore(r);
        }
        RestorePinDevices(r);
    }

    void PostRestore() override {
        std::lock_guard<std::mutex> lk(mtx_);
        DriveIcuLocked();
    }

private:
    static uint32_t HelperOffset(uint32_t off) {
        switch (off) {
            case kOffGintst: return kOffIntStatL;
            case kOffGinten: return kOffIntEnL;
            case kOffGcints: return kOffIntTypL;
            case kOffGlints: return kOffIntAlSelL;
            default:         return 0xFFFFFFFFu;
        }
    }

    static int RegisterBit(int pin) { return pin == kDcdPin ? kDcdBit : pin; }

    bool TclockLocked() const { return cmu_->SuppliedClocks(kCmuMskGiu) != 0u; }

    uint32_t EffectivePinsLocked() const { return inputs_.Merged(out_, gouten_); }

    uint32_t DetectingPinsLocked(bool tclock) const {
        const uint32_t clocked = tclock ? kPinBits : kMasterOutPins;
        return clocked & ~static_cast<uint32_t>(gouten_);
    }

    void ApplyPinLevelLocked(int pin, int bit, Vr41xxGiuPinEdge edge) {
        pmu_->OnGpioLevel(pin, edge.prev, edge.level);
        if (((DetectingPinsLocked(TclockLocked()) >> bit) & 1u) != 0u) irq_.OnPinLevel(bit, edge.prev, edge.level);
        DriveIcuLocked();
    }

    void EvaluateLevelsLocked(bool tclock) {
        irq_.EvaluateLevels(inputs_.Established() & DetectingPinsLocked(tclock), inputs_.Levels());
        DriveIcuLocked();
    }

    void RequireInputInterruptsLocked() {
        uint16_t inten = 0u;
        irq_.ReadRegister(kOffIntEnL, &inten);
        if ((inten & gouten_) != 0u) {
            emu_.Get<Fatal>().Die("Vr4101Giu: GINTENREG 0x%04X enables an interrupt on a GOUTENREG 0x%04X "
                                  "output pin", inten, gouten_);
        }
    }

    void DriveIcuLocked() { icu_->SetGiuLow(irq_.IcuLow(false)); }

    void ResetUnit() {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            gouten_ = 0u;
            out_    = 0u;
            irq_.Reset();
            EvaluateLevelsLocked(TclockLocked());
        }
        NotifyOutputDriveChanged();
    }

    std::mutex          mtx_;
    Vr41xxPmu*          pmu_ = nullptr;
    Vr41xxIcu*          icu_ = nullptr;
    Vr41xxCmu*          cmu_ = nullptr;
    uint16_t            gouten_ = 0u;
    uint16_t            out_    = 0u;
    Vr41xxGiuInterrupts irq_{false, true};
    Vr41xxGiuInputPins  inputs_{0u};
};

}

REGISTER_SERVICE_AS(Vr4101Giu, Vr41xxGiu);

}
