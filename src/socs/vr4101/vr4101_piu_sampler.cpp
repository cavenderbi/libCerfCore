#include "vr4101_piu_sampler.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "vr4101_interval_unit.h"
#include "vr4101_piu_inputs.h"
#include "vr4101_piu_regs.h"

namespace cerfcore {

using namespace vr4101_piu;

namespace {

constexpr uint16_t kStableMask            = 0x000Fu;
constexpr uint16_t kSelAdClkMask          = 0x00F0u;
constexpr uint32_t kSelAdClkShift         = 4u;
constexpr uint64_t kAdClocksPerConversion = 10u;
constexpr uint64_t kConversionNs          = 21000u;
constexpr uint64_t kNsPerSecond           = 1000000000u;
constexpr uint16_t kAdCmdRfu              = 0x000Fu;
constexpr uint16_t kNoPanelDrive   = 0x1Bu;
constexpr uint16_t kAinMainBattery = 0u;
constexpr uint16_t kAinSubBattery  = 2u;
constexpr uint16_t kTouchSetLen    = 4u;
constexpr uint16_t kOtherProtocolSample = 0u;

constexpr Vr4101PiuSampler::Datum kPenStateDatum = {3u, 0x1Eu};
constexpr Vr4101PiuSampler::Datum kTouchSet[]    = {{1u, 0x19u}, {1u, 0x0Bu}, {3u, 0x1Au}, {3u, 0x13u}};

}

void Vr4101PiuSampler::Attach(uint64_t tclk_num, uint64_t tclk_den) {
    inputs_           = &emu_.Get<Vr4101PiuInputs>();
    conversion_ticks_ = (kConversionNs * tclk_num + kNsPerSecond * tclk_den - 1u) / (kNsPerSecond * tclk_den);
}

uint16_t Vr4101PiuSampler::SetLength(uint16_t state, uint16_t cnt) const {
    if (state != kStPenScan) return 1u;
    return static_cast<uint16_t>(kTouchSetLen + ((cnt & kScanType) ? 1u : 0u));
}

uint64_t Vr4101PiuSampler::CoordinateSetTclocks(uint16_t stbl, uint16_t cnt) const {
    const uint64_t stable = static_cast<uint64_t>(stbl & kStableMask) * kVr4101TclocksPerIntervalUnit;
    const uint64_t ad_clk = 4u * ((stbl & kSelAdClkMask) >> kSelAdClkShift) + 2u;
    return (stable + kAdClocksPerConversion * ad_clk + conversion_ticks_) * (SetLength(kStPenScan, cnt) + 1u);
}

Vr4101PiuSampler::Datum Vr4101PiuSampler::SetDatum(uint16_t state, uint16_t cnt, uint16_t cmd, uint16_t index) const {
    if (state == kStPenScan) {
        if (!(cnt & kScanType)) return kTouchSet[index];
        return index == 0u ? kPenStateDatum : kTouchSet[index - 1u];
    }
    if (state == kStCommand) {
        if ((cmd & kAdCmdMask) == kAdCmdRfu) {
            emu_.Get<Fatal>().Die("VR4101 PIU command scan with RFU ADCMD (PIUCMDREG 0x%04X)", cmd);
        }
        return {static_cast<uint16_t>(cmd & kAdCmdMask), static_cast<uint16_t>((cmd >> kPenCntShift) & kPenCntBits)};
    }
    return {state == kStMainBatt ? kAinMainBattery : kAinSubBattery, kNoPanelDrive};
}

uint16_t Vr4101PiuSampler::Convert(const Datum& d, uint16_t stbl, const Vr41xxPiuPenPoint& pen) {
    const uint32_t model_bits = (stbl & kStblModel) ? 10u : 12u;
    if (model_bits != inputs_->ConverterBits()) return kOtherProtocolSample;
    return inputs_->Convert(d.ain, d.pencont, pen);
}

}
