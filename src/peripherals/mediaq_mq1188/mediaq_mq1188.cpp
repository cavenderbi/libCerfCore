#include "mediaq_mq1188.h"

#include "../../core/board_parts.h"
#include "mediaq_mq1188_part_id.h"
#include <cerfcore/core/byte_order.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/log.h>
#include "../peripheral_dispatcher.h"
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "mediaq_mq1188_board_pins.h"

#include <cstring>

namespace cerfcore {

namespace {

struct ResetBits {
    uint32_t first;
    uint32_t last;
    uint32_t mask;
    uint32_t value;
};

constexpr uint32_t kAll = 0xFFFFFFFFu;

constexpr ResetBits kResetBits[] = {
    {0x000u, 0x010u, kAll,    0u},
    {0x080u, 0x088u, 0xFu,    0u},
    {0x100u, 0x10Cu, kAll,    0u},
    {0x180u, 0x180u, kAll,    0u},
    {0x184u, 0x184u, 0xFFu,   0u},
    {0x198u, 0x19Cu, 0xFFFFu, 0u},
    {0x200u, 0x200u, kAll,    0u},
    {0x230u, 0x230u, kAll,    0u},
    {0x280u, 0x284u, kAll,    0u},
    {0x290u, 0x290u, 0xFu,    0u},
    {0x2A0u, 0x2ACu, 0xFFu,   0u},
    {0x2C0u, 0x2C4u, kAll,    0u},
    {0x2CCu, 0x2CCu, kAll,    0u},
    {0x300u, 0x320u, kAll,    0u},
    {0x380u, 0x380u, kAll,    0u},
    {0x384u, 0x384u, kAll,    0xF0000000u},
    {0x388u, 0x39Cu, kAll,    0u},
    {0x600u, 0x604u, kAll,    0u},
    {0x608u, 0x608u, kAll,    0x0FFCFCFFu},
    {0x60Cu, 0x61Cu, kAll,    0u},
    {0x628u, 0x628u, kAll,    0xF0030000u},
    {0x62Cu, 0x62Cu, 0xFFFu,  0u},
    {0x638u, 0x638u, 0xFFu,   0u},
    {0x63Cu, 0x63Cu, kAll,    0u},
    {0x1000u, 0x1028u, kAll,  0u},
    {0x102Cu, 0x102Cu, 0xFFu, 0u},
    {0x1030u, 0x1048u, kAll,  0u},
    {0x1060u, 0x107Cu, kAll,  0u},
};

}

const OhciProfile MediaQMq1188::kUsbProfile = {
    "MediaQ MQ1188",
    0x00000010u,
    0xC000007Fu,
    0x000007FFu,
    0x00001102u,
    0xFF001B00u,
    0x00000628u,
};

bool MediaQMq1188::ShouldRegister() {
    return emu_.Get<BoardParts>().Has(PartId::MediaqMq1188);
}

Fatal& MediaQMq1188::OwnerFatal() {
    return emu_.Get<Fatal>();
}

void MediaQMq1188::OnReady() {
    sram_.assign(kSramSize, 0u);
    ResetRegisters();
    ohci_.Attach();
    UpdateUsbClock();
    UpdateInterruptPin(true);
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) {
        ResetRegisters();
        in02_latched_ = 0u;
        ohci_.HardwareReset();
        UpdateUsbClock();
        UpdateInterruptPin(true);
        mode_latch_.Publish(emu_, false, 0u, 0u);
        ge_.PowerOnReset();
    });
    emu_.Get<PeripheralDispatcher>().Register(this);
}

void MediaQMq1188::ResetRegisters() {
    for (const ResetBits& b : kResetBits) {
        for (uint32_t off = b.first; off <= b.last; off += 4u) {
            reg_[off / 4u] = (reg_[off / 4u] & ~b.mask) | b.value;
        }
    }
}

bool MediaQMq1188::IsEnabled() const {
    /* GC08R/GC09R reset to 0, which the (n-1) geometry encoding reads back as a
       1-pixel dimension; require >1 so the enable edge fires on the programmed
       mode, not the transient state before the mode-set writes geometry. */
    return (Reg(kDc05R) & 0x1u) != 0u && GetGuestW() > 1u &&
           GetGuestH() > 1u && Stride() != 0u;
}

uint32_t MediaQMq1188::Bpp() const {
    switch ((Reg(kGc00R) >> 4u) & 0xFu) {  /* GC00R[7:4] color depth (Reg 4-31). */
        case 0x0u: return 1u;
        case 0x1u: return 2u;
        case 0x2u: return 4u;
        case 0x3u: return 8u;
        case 0x4u:
        case 0xCu: return 16u;
        default:   return 0u;
    }
}

uint32_t MediaQMq1188::GetGuestW()      const { return ((Reg(kGc08R) >> 16u) & 0xFFFFu) + 1u; }
uint32_t MediaQMq1188::GetGuestH()      const { return ((Reg(kGc09R) >> 16u) & 0xFFFFu) + 1u; }
uint32_t MediaQMq1188::FbWindowOffset() const { return Reg(kGc0CR) & 0x3FFFFu; }
uint32_t MediaQMq1188::Stride()         const { return Reg(kGc0ER) & 0x3FFFFu; }

uint32_t MediaQMq1188::PaletteEntry(uint32_t index) const {
    return Reg(kPaletteBase + (index & 0xFFu) * 4u);
}

void MediaQMq1188::PublishScreenSizeOnEnableEdge() {
    const bool on = IsEnabled();
    const uint32_t w = on ? GetGuestW() : 0u, h = on ? GetGuestH() : 0u;
    if (!mode_latch_.Publish(emu_, on, w, h)) return;
    LOG(Lcd, "MediaQMq1188: display enabled %ux%u %ubpp stride=%u fb_off=0x%X\n",
        w, h, Bpp(), Stride(), FbWindowOffset());
}

uint32_t MediaQMq1188::RegRead(uint32_t addr) {
    const uint32_t roff = (addr - MmioBase()) - kRegBase;
    if (roff == kCc01R) { ge_.FlushPending(); return ge_.StatusReady(); }
    if (roff >= kGeCmdLo && roff < kGeCmdHi) return ge_.ReadReg((roff - kGeCmdLo) / 4u);
    if (IsUsbHost(roff)) return ohci_.Read(roff - kUsbLo);
    if (roff == kIn02R) return InterruptStatus();
    if (roff == kIn03R) return RawInterruptStatus();
    if (roff == kCc02R) return ReadCc02R();
    return Reg(roff);
}

void MediaQMq1188::RegWrite(uint32_t addr, uint32_t value) {
    const uint32_t roff = (addr - MmioBase()) - kRegBase;
    /* GE registers are reached only via the queued alias (0x1400). The 0x200
       block is the GE direct block on the MQ-1132 but the SD/MMC controller on
       the MQ-1188 (mq1188sdmmc.dll maps PA 0x08040200, writes DMA descriptors at
       0x22C) - routing it to the GE corrupts GE0BR. SD/MMC not yet modelled. */
    if (roff >= kGeCmdLo && roff < kGeCmdHi) { ge_.WriteReg((roff - kGeCmdLo) / 4u, value); return; }
    if (roff >= kSrcFifoLo && roff < kSrcFifoHi) { ge_.PushSourceFifo(value); return; }
    if (IsUsbHost(roff)) { ohci_.Write(roff - kUsbLo, value); return; }
    if (roff == kIn02R) { WriteInterruptStatus(value); return; }
    if (roff == kIn03R) OwnerFatal().Die("MediaQMq1188: IN03R write val=0x%08X", value);
    reg_[roff / 4u] = value;
    if (roff == kIn00R || roff == kIn01R || roff == kCc02R) {
        LatchGpioStatus();
        UpdateInterruptPin(false);
    }
    if (roff == kDc05R) UpdateUsbClock();
    PublishScreenSizeOnEnableEdge();
}

void MediaQMq1188::RequireWordRegisterAccess(uint32_t off, uint32_t addr, uint32_t value) {
    const uint32_t roff = (off - kRegBase) & ~0x3u;
    if (IsUsbHost(roff) || roff == kIn02R) HaltUnsupportedAccess("sub-word access to a word-only register", addr, value);
}

uint8_t* MediaQMq1188::UsbBusMemory(uint32_t addr) {
    const uint32_t off = addr - MmioBase();
    if (off >= kApertureSize || InRegWindow(off) || (off & 0x3u) != 0u)
        OwnerFatal().Die("MediaQMq1188 USB host: bus master access outside the SRAM aperture addr=0x%08X", addr);
    return &sram_[SramOffset(off)];
}

uint32_t MediaQMq1188::OhciRead32(uint32_t addr) { return cerf::le::U32(UsbBusMemory(addr)); }

void MediaQMq1188::OhciWrite32(uint32_t addr, uint32_t value) { cerf::le::Put32(UsbBusMemory(addr), value); }

void MediaQMq1188::OhciInterrupt(bool asserted) {
    usb_interrupt_ = asserted;
    UpdateInterruptPin(false);
}

void MediaQMq1188::UpdateUsbClock() { ohci_.SetClockEnabled((Reg(kDc05R) & kDc05rUsbHostClock) != 0u); }

uint32_t MediaQMq1188::InterruptStatus() const { return in02_latched_ | (usb_interrupt_ ? kIntUsbHost : 0u); }

bool MediaQMq1188::GpioInputEnabled(uint32_t gpio) const {
    return ((Reg(kCc02R) >> (gpio * kCc02rGpioField)) & kCc02rInputEnable) != 0u;
}

uint32_t MediaQMq1188::ReadCc02R() {
    uint32_t value = Reg(kCc02R);
    for (uint32_t gpio = 0; gpio < kMqGpioPins; ++gpio) {
        if (!GpioInputEnabled(gpio)) continue;
        const uint32_t data = kCc02rData << (gpio * kCc02rGpioField);
        value = emu_.Get<MediaQMq1188BoardPins>().GpioInputHigh(gpio) ? (value | data) : (value & ~data);
    }
    return value;
}

bool MediaQMq1188::GpioPinHigh(uint32_t gpio) {
    const uint32_t field = Reg(kCc02R) >> (gpio * kCc02rGpioField);
    if ((field & kCc02rInputEnable) != 0u) return emu_.Get<MediaQMq1188BoardPins>().GpioInputHigh(gpio);
    if ((field & kCc02rTestSource) != 0u)
        OwnerFatal().Die("MediaQMq1188: GPIO %u driven by the test-mode internal signal CC02R=0x%08X", gpio,
                         Reg(kCc02R));
    return (field & kCc02rData) != 0u;
}

bool MediaQMq1188::GpioEventTrue(uint32_t gpio) {
    const uint32_t field = Reg(kCc02R) >> (gpio * kCc02rGpioField);
    if ((field & (kCc02rInputEnable | kCc02rOutputEnable)) == 0u) return false;
    return GpioPinHigh(gpio) == ((Reg(kIn00R) & (kIn00rGpioLevel0 << gpio)) != 0u);
}

uint32_t MediaQMq1188::RawInterruptStatus() {
    ge_.FlushPending();
    const uint32_t cc01     = ge_.StatusReady();
    const uint32_t cmd_free = cc01 & kCc01rFifoFree;
    const uint32_t src_free = (cc01 >> kCc01rSourceShift) & kCc01rFifoFree;
    uint32_t       raw      = 0u;
    for (uint32_t gpio = 0; gpio < kMqGpioPins; ++gpio)
        if (GpioEventTrue(gpio)) raw |= kRawGpio0 << gpio;
    if ((cc01 & kCc01rGeBusy) != 0u) raw |= kRawGeBusy;
    if (src_free == kFifoDepth) raw |= kRawSourceEmpty;
    if (src_free >= kFifoDepth / 2u) raw |= kRawSourceHalf;
    if (cmd_free == kFifoDepth) raw |= kRawCommandEmpty;
    if (cmd_free >= kFifoDepth / 2u) raw |= kRawCommandHalf;
    if (usb_interrupt_) raw |= kRawUsbHost;
    return raw;
}

void MediaQMq1188::LatchGpioStatus() {
    for (uint32_t gpio = 0; gpio < kMqGpioPins; ++gpio) {
        const uint32_t bit = kIntGpio0 << gpio;
        if ((Reg(kIn01R) & bit) != 0u && GpioEventTrue(gpio)) in02_latched_ |= bit;
    }
}

void MediaQMq1188::WriteInterruptStatus(uint32_t value) {
    if ((value & ~kIntDefined) != 0u)
        OwnerFatal().Die("MediaQMq1188: IN02R write to reserved bits val=0x%08X", value);
    in02_latched_ &= ~(value & kIntLatched);
    LatchGpioStatus();
    UpdateInterruptPin(false);
}

void MediaQMq1188::UpdateInterruptPin(bool force) {
    const uint32_t in00     = Reg(kIn00R);
    const bool     asserted = (in00 & kIn00rEnable) != 0u && (InterruptStatus() & Reg(kIn01R) & kIntDefined) != 0u;
    const bool     high     = (in00 & kIn00rActiveHigh) != 0u ? asserted : !asserted;
    if (!force && high == pin_high_) return;
    pin_high_ = high;
    emu_.Get<MediaQMq1188BoardPins>().SetInterruptLevel(high);
}

uint8_t MediaQMq1188::ReadByte(uint32_t addr) {
    const uint32_t off = addr - MmioBase();
    if (InRegWindow(off)) {
        RequireWordRegisterAccess(off, addr, 0u);
        const uint32_t word = RegRead(addr & ~0x3u);
        return static_cast<uint8_t>(word >> ((off & 0x3u) * 8u));
    }
    return sram_[SramOffset(off)];
}

uint16_t MediaQMq1188::ReadHalf(uint32_t addr) {
    const uint32_t off = addr - MmioBase();
    if (InRegWindow(off)) {
        RequireWordRegisterAccess(off, addr, 0u);
        const uint32_t word = RegRead(addr & ~0x3u);
        return static_cast<uint16_t>(word >> ((off & 0x2u) * 8u));
    }
    return cerf::le::U16(&sram_[SramOffset(off)]);
}

uint32_t MediaQMq1188::ReadWord(uint32_t addr) {
    const uint32_t off = addr - MmioBase();
    if (InRegWindow(off)) return RegRead(addr);
    return cerf::le::U32(&sram_[SramOffset(off)]);
}

void MediaQMq1188::WriteByte(uint32_t addr, uint8_t value) {
    const uint32_t off = addr - MmioBase();
    if (InRegWindow(off)) {
        RequireWordRegisterAccess(off, addr, value);
        const uint32_t shift = (off & 0x3u) * 8u;
        const uint32_t word  = RegRead(addr & ~0x3u);
        RegWrite(addr & ~0x3u,
                 (word & ~(0xFFu << shift)) | (static_cast<uint32_t>(value) << shift));
        return;
    }
    sram_[SramOffset(off)] = value;
}

void MediaQMq1188::WriteHalf(uint32_t addr, uint16_t value) {
    const uint32_t off = addr - MmioBase();
    if (InRegWindow(off)) {
        RequireWordRegisterAccess(off, addr, value);
        const uint32_t shift = (off & 0x2u) * 8u;
        const uint32_t word  = RegRead(addr & ~0x3u);
        RegWrite(addr & ~0x3u,
                 (word & ~(0xFFFFu << shift)) | (static_cast<uint32_t>(value) << shift));
        return;
    }
    cerf::le::Put16(&sram_[SramOffset(off)], value);
}

void MediaQMq1188::WriteWord(uint32_t addr, uint32_t value) {
    const uint32_t off = addr - MmioBase();
    if (InRegWindow(off)) { RegWrite(addr, value); return; }
    cerf::le::Put32(&sram_[SramOffset(off)], value);
}

void MediaQMq1188::SaveState(StateWriter& w) {
    w.WriteBytes("sram", sram_.data(), sram_.size());
    w.WriteBytes("reg", reg_, sizeof(reg_));
    mode_latch_.SaveState(w);
    ge_.SaveState(w);
    ohci_.SaveState(w);
    w.Write("mq_in02_latched", in02_latched_);
}

void MediaQMq1188::RestoreState(StateReader& r) {
    r.ReadBytes("sram", sram_.data(), sram_.size());
    r.ReadBytes("reg", reg_, sizeof(reg_));
    mode_latch_.RestoreState(r);
    ge_.RestoreState(r);
    ohci_.RestoreState(r);
    r.Read("mq_in02_latched", in02_latched_);
}

void MediaQMq1188::PostRestore() {
    ohci_.PostRestore();
    UpdateInterruptPin(true);
}

REGISTER_SERVICE(MediaQMq1188);

}
