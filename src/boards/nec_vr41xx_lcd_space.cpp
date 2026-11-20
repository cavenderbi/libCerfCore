#include "nec_vr41xx_lcd_space.h"

#include <cerfcore/core/byte_order.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/log.h>
#include "../peripherals/peripheral_dispatcher.h"
#include "../peripherals/pin_drive.h"
#include "../socs/vr41xx/vr41xx_giu.h"
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>

namespace cerfcore {

bool NecVr41xxLcdSpace::ShouldRegister() {
    return emu_.TryGet<NecVr41xxLcdPanel>() != nullptr;
}

void NecVr41xxLcdSpace::OnReady() {
    panel_ = &emu_.Get<NecVr41xxLcdPanel>();
    giu_   = &emu_.Get<Vr41xxGiu>();
    fb_.assign(panel_->VramBytes(), 0u);
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) { size_latch_.Reset(); });
    giu_->RegisterOutputDriveListener([this] { PublishScreenSizeOnDisplayOn(); });
    emu_.Get<PeripheralDispatcher>().Register(this);
}

bool NecVr41xxLcdSpace::DisplayOn() const {
    return giu_->OutputDrive(panel_->DisplayEnableGpio()) == PinDrive::kHigh;
}

void NecVr41xxLcdSpace::PublishScreenSizeOnDisplayOn() {
    if (!DisplayOn()) return;
    if (size_latch_.PublishOnce(emu_, true)) {
        LOG(Lcd, "NecVr41xxLcdSpace: frame buffer %ux%u %ubpp stride=%u\n",
            GuestW(), GuestH(), Bpp(), StrideBytes());
    }
}

uint8_t NecVr41xxLcdSpace::ReadByte(uint32_t addr) {
    const uint32_t off = addr - MmioBase();
    if (off < fb_.size()) return fb_[off];
    HaltUnsupportedAccess("NecVr41xxLcdSpace ReadByte", addr, 0);
}

uint16_t NecVr41xxLcdSpace::ReadHalf(uint32_t addr) {
    const uint32_t off = addr - MmioBase();
    if (off + 1u < fb_.size()) return cerf::le::U16(fb_.data(), off);
    HaltUnsupportedAccess("NecVr41xxLcdSpace ReadHalf", addr, 0);
}

uint32_t NecVr41xxLcdSpace::ReadWordAsHalves(uint32_t addr) {
    const uint32_t off = addr - MmioBase();
    if (off + 3u < fb_.size()) return cerf::le::U32(fb_.data(), off);
    return Peripheral::ReadWordAsHalves(addr);
}

void NecVr41xxLcdSpace::WriteByte(uint32_t addr, uint8_t value) {
    const uint32_t off = addr - MmioBase();
    if (off < fb_.size()) { fb_[off] = value; return; }
    HaltUnsupportedAccess("NecVr41xxLcdSpace WriteByte", addr, value);
}

void NecVr41xxLcdSpace::WriteHalf(uint32_t addr, uint16_t value) {
    const uint32_t off = addr - MmioBase();
    if (off + 1u < fb_.size()) { cerf::le::Put16(fb_.data() + off, value); return; }
    HaltUnsupportedAccess("NecVr41xxLcdSpace WriteHalf", addr, value);
}

void NecVr41xxLcdSpace::WriteWordAsHalves(uint32_t addr, uint32_t value) {
    const uint32_t off = addr - MmioBase();
    if (off + 3u < fb_.size()) { cerf::le::Put32(fb_.data() + off, value); return; }
    Peripheral::WriteWordAsHalves(addr, value);
}

void NecVr41xxLcdSpace::SaveState(StateWriter& w) {
    w.WriteBytes("fb_pin", fb_.data(), fb_.size());
    size_latch_.SaveState(w);
}

void NecVr41xxLcdSpace::RestoreState(StateReader& r) {
    r.ReadBytes("fb_pin", fb_.data(), fb_.size());
    size_latch_.RestoreState(r);
}

REGISTER_SERVICE(NecVr41xxLcdSpace);

}
