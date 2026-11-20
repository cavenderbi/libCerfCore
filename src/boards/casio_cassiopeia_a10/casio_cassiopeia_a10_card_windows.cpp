#include "casio_cassiopeia_a10_card_windows.h"

#include <cerfcore/core/cerf_emulator.h>
#include "../../peripherals/pcmcia/pcmcia_slot.h"
#include "../../peripherals/peripheral_dispatcher.h"

namespace cerfcore {

namespace {

constexpr uint32_t kSpaces     = 3u;
constexpr uint32_t kOffsetMask = CasioCassiopeiaA10CardWindows::kSpaceBytes - 1u;

}

void CasioCassiopeiaA10CardWindows::OnReady() {
    slot_ = &CardSlot();
    emu_.Get<PeripheralDispatcher>().Register(this);
}

uint32_t CasioCassiopeiaA10CardWindows::MmioSize() const { return kSpaces * kSpaceBytes; }

bool CasioCassiopeiaA10CardWindows::SpaceSelected(Space) { return true; }

CasioCassiopeiaA10CardWindows::Space CasioCassiopeiaA10CardWindows::SpaceOf(uint32_t addr, const char* access,
                                                                            uint32_t value) {
    const Space space = static_cast<Space>((addr - MmioBase()) / kSpaceBytes);
    if (slot_->HasCard() && slot_->IsPowered() && !SpaceSelected(space)) HaltUnsupportedAccess(access, addr, value);
    return space;
}

uint8_t CasioCassiopeiaA10CardWindows::ReadByte(uint32_t addr) {
    const uint32_t off = addr & kOffsetMask;
    switch (SpaceOf(addr, "ReadByte", 0u)) {
    case Space::kAttribute:
        return slot_->ReadAttribute8(off);
    case Space::kCommon:
        return slot_->ReadCommon8(off);
    case Space::kIo:
        break;
    }
    return slot_->ReadIo8(off);
}

void CasioCassiopeiaA10CardWindows::WriteByte(uint32_t addr, uint8_t value) {
    const uint32_t off = addr & kOffsetMask;
    switch (SpaceOf(addr, "WriteByte", value)) {
    case Space::kAttribute:
        slot_->WriteAttribute8(off, value);
        return;
    case Space::kCommon:
        slot_->WriteCommon8(off, value);
        return;
    case Space::kIo:
        slot_->WriteIo8(off, value);
        return;
    }
}

uint16_t CasioCassiopeiaA10CardWindows::ReadHalf(uint32_t addr) {
    const uint32_t off = addr & kOffsetMask;
    switch (SpaceOf(addr, "ReadHalf", 0u)) {
    case Space::kAttribute:
        return slot_->ReadAttribute16(off);
    case Space::kCommon:
        return slot_->ReadCommon16(off);
    case Space::kIo:
        break;
    }
    return slot_->ReadIo16(off);
}

void CasioCassiopeiaA10CardWindows::WriteHalf(uint32_t addr, uint16_t value) {
    const uint32_t off = addr & kOffsetMask;
    switch (SpaceOf(addr, "WriteHalf", value)) {
    case Space::kAttribute:
        slot_->WriteAttribute16(off, value);
        return;
    case Space::kCommon:
        slot_->WriteCommon16(off, value);
        return;
    case Space::kIo:
        slot_->WriteIo16(off, value);
        return;
    }
}

}
