#include "casio_cassiopeia_a10_touch_adc.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_request_lines.h"

namespace cerfcore {

REGISTER_SERVICE(CasioCassiopeiaA10TouchAdc);

namespace {

constexpr uint32_t kSingleResult = 0x10000080u;
constexpr uint32_t kPowerState   = 0x10000082u;
constexpr uint32_t kStatus       = 0x10000084u;
constexpr uint32_t kChannel      = 0x10000086u;
constexpr uint32_t kSelect       = 0x10000088u;
constexpr uint32_t kPenState     = 0x1000008Au;
constexpr uint32_t kSequence     = 0x1000008Cu;
constexpr uint32_t kFirstSetup   = 0x10000092u;
constexpr uint32_t kFirstSample  = 0x10000098u;
constexpr uint32_t kBlockEnd     = 0x1000009Eu;

constexpr uint16_t kPowerOff         = 0x0000u;
constexpr uint16_t kPowerFirstStep   = 0x0001u;
constexpr uint16_t kPowerOn          = 0x0002u;
constexpr uint16_t kSingleDone       = 0x0001u;
constexpr uint16_t kSequenceDone     = 0x0002u;
constexpr uint16_t kClearableStatus  = 0x0007u;
constexpr uint16_t kStoredBit        = 0x0008u;
constexpr uint16_t kSelectSingle     = 0x0000u;
constexpr uint16_t kSelectSequence   = 0x0001u;
constexpr uint16_t kSampledChannel   = 0x0004u;
constexpr uint16_t kPlateX           = 0x0002u;
constexpr uint16_t kPlateY           = 0x0003u;
constexpr uint16_t kPenDownLevel     = 0x0000u;
constexpr uint16_t kPenUpLevel       = 0x0001u;
constexpr uint16_t kSingleSample     = 0x0000u;
constexpr uint8_t  kAllSetupWritten  = 0x07u;

constexpr HostPenInput::PanelRange kPanel{0x0029u, 0x035Cu, 0x0078u, 0x03DAu};

}

bool CasioCassiopeiaA10TouchAdc::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10;
}

void CasioCassiopeiaA10TouchAdc::OnReady() {
    requests_ = &emu_.Get<CasioCassiopeiaA10RequestLines>();
    emu_.Get<PeripheralDispatcher>().Register(this);
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) { Reset(); });
}

uint32_t CasioCassiopeiaA10TouchAdc::MmioBase() const { return kSingleResult; }
uint32_t CasioCassiopeiaA10TouchAdc::MmioSize() const { return kBlockEnd - kSingleResult; }

uint16_t CasioCassiopeiaA10TouchAdc::ReadHalf(uint32_t addr) {
    switch (addr) {
    case kSingleResult:
        if (!single_ready_) HaltUnsupportedAccess("ReadHalf", addr, 0u);
        return single_result_;
    case kStatus:
        return status_;
    case kPenState:
        return PenDetected() ? kPenDownLevel : kPenUpLevel;
    default:
        break;
    }
    if (addr >= kFirstSetup && addr < kFirstSample) {
        if ((addr & 1u) != 0u) HaltUnsupportedAccess("ReadHalf", addr, 0u);
        return setup_[(addr - kFirstSetup) / 2u];
    }
    const uint32_t index = (addr - kFirstSample) / 2u;
    if (addr < kFirstSample || (addr & 1u) != 0u || index >= kSamples || !samples_ready_) {
        HaltUnsupportedAccess("ReadHalf", addr, 0u);
    }
    return samples_[index];
}

void CasioCassiopeiaA10TouchAdc::WriteHalf(uint32_t addr, uint16_t value) {
    switch (addr) {
    case kPowerState:
        if (value != kPowerOff && value != kPowerFirstStep && value != kPowerOn) {
            HaltUnsupportedAccess("WriteHalf", addr, value);
        }
        power_state_ = value;
        DrivePenLine();
        return;
    case kStatus:
        if ((value & ~(kClearableStatus | kStoredBit)) != 0u) HaltUnsupportedAccess("WriteHalf", addr, value);
        status_ = static_cast<uint16_t>(((status_ & kClearableStatus) & ~value) | (value & kStoredBit));
        return;
    case kChannel:
        StartSingle(addr, value);
        return;
    case kSelect:
        if (value != kSelectSingle && value != kSelectSequence) HaltUnsupportedAccess("WriteHalf", addr, value);
        conversion_select_   = value;
        conversion_selected_ = true;
        return;
    case kSequence:
        StartSequence(addr, value);
        return;
    default:
        WriteSetup(addr, value);
        return;
    }
}

void CasioCassiopeiaA10TouchAdc::SaveState(StateWriter& w) {
    w.Write("power_state", power_state_);
    w.Write("status", status_);
    w.Write("conversion_select", conversion_select_);
    w.Write("conversion_selected", conversion_selected_);
    w.Write("setup_written", setup_written_);
    w.Write("setup", setup_);
    w.Write("single_ready", single_ready_);
    w.Write("single_result", single_result_);
    w.Write("samples_ready", samples_ready_);
    w.Write("samples", samples_);
}

void CasioCassiopeiaA10TouchAdc::RestoreState(StateReader& r) {
    r.Read("power_state", power_state_);
    r.Read("status", status_);
    r.Read("conversion_select", conversion_select_);
    r.Read("conversion_selected", conversion_selected_);
    r.Read("setup_written", setup_written_);
    r.Read("setup", setup_);
    r.Read("single_ready", single_ready_);
    r.Read("single_result", single_result_);
    r.Read("samples_ready", samples_ready_);
    r.Read("samples", samples_);
}

void CasioCassiopeiaA10TouchAdc::ApplyPen(const HostPenInput::Pen& pen) {
    host_pen_down_ = pen.kind == HostPenInput::kPenDown;
    DrivePenLine();
}

void CasioCassiopeiaA10TouchAdc::ResetPen() {
    host_pen_down_ = false;
    DrivePenLine();
}

bool CasioCassiopeiaA10TouchAdc::PenDetected() const { return host_pen_down_ && power_state_ == kPowerOn; }

void CasioCassiopeiaA10TouchAdc::DrivePenLine() {
    requests_->Drive(CasioCassiopeiaA10Request::kPen, PenDetected());
}

void CasioCassiopeiaA10TouchAdc::StartSingle(uint32_t addr, uint16_t channel) {
    if (channel != kSampledChannel) HaltUnsupportedAccess("WriteHalf", addr, channel);
    if (power_state_ != kPowerOn || !conversion_selected_ || conversion_select_ != kSelectSingle) {
        emu_.Get<Fatal>().Die("CasioCassiopeiaA10TouchAdc: single conversion of channel %u with power state %u, select %u",
                              channel, power_state_, conversion_selected_ ? conversion_select_ : 0xFFFFu);
    }
    single_result_ = kSingleSample;
    single_ready_  = true;
    status_        = static_cast<uint16_t>(status_ | kSingleDone);
}

void CasioCassiopeiaA10TouchAdc::StartSequence(uint32_t addr, uint16_t plate) {
    if (plate != kPlateX && plate != kPlateY) HaltUnsupportedAccess("WriteHalf", addr, plate);
    if (power_state_ != kPowerOn || !conversion_selected_ || conversion_select_ != kSelectSequence ||
        setup_written_ != kAllSetupWritten) {
        emu_.Get<Fatal>().Die("CasioCassiopeiaA10TouchAdc: sequence on plate %u with power state %u, select %u, setup mask %u",
                              plate, power_state_, conversion_selected_ ? conversion_select_ : 0xFFFFu, setup_written_);
    }
    const uint16_t sample = PlatePosition(kPanel, plate == kPlateX ? Plate::kX : Plate::kY);
    for (uint16_t& s : samples_) s = sample;
    samples_ready_ = true;
    status_        = static_cast<uint16_t>(status_ | kSequenceDone);
}

void CasioCassiopeiaA10TouchAdc::WriteSetup(uint32_t addr, uint16_t value) {
    const uint32_t index = (addr - kFirstSetup) / 2u;
    if (addr < kFirstSetup || (addr & 1u) != 0u || index >= kSetupFields) HaltUnsupportedAccess("WriteHalf", addr, value);
    setup_[index]  = value;
    setup_written_ = static_cast<uint8_t>(setup_written_ | (1u << index));
}

void CasioCassiopeiaA10TouchAdc::Reset() {
    power_state_         = kPowerOff;
    status_              = 0u;
    conversion_select_   = 0u;
    conversion_selected_ = false;
    setup_written_       = 0u;
    for (uint16_t& v : setup_) v = 0u;
    single_ready_        = false;
    single_result_       = 0u;
    samples_ready_       = false;
    for (uint16_t& v : samples_) v = 0u;
    DrivePenLine();
}

}
