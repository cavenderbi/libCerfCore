#include "intel_28f256k3.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/state/state_stream.h>

namespace cerfcore {

namespace {

constexpr uint8_t kLocked     = 0x01u;
constexpr uint8_t kLockedDown = 0x02u;

constexpr uint32_t kIdManufacturer   = 0x00u;
constexpr uint32_t kIdDevice         = 0x01u;
constexpr uint32_t kIdBlockStatus    = 0x02u;
constexpr uint32_t kIdConfiguration  = 0x05u;
constexpr uint32_t kIdPrLock0        = 0x80u;
constexpr uint32_t kIdFactoryPrFirst = 0x81u;
constexpr uint32_t kIdFactoryPrLast  = 0x84u;
constexpr uint32_t kIdOtpLast        = 0x109u;

constexpr uint16_t kPrLock0FactoryProgrammed = 0xFFFEu;
constexpr uint16_t kFactoryUniqueWord        = 0x0000u;
constexpr uint16_t kOtpErased                = 0xFFFFu;

bool IsK3Command(uint8_t cmd) {
    switch (cmd) {
        case 0xFFu: case 0x10u: case 0x40u: case 0xE8u: case 0x30u:
        case 0x20u: case 0xD0u: case 0xB0u: case 0x70u: case 0x50u:
        case 0x90u: case 0xB8u: case 0x98u: case 0x60u: case 0xC0u:
            return true;
    }
    return false;
}

}

void Intel28F256K3::OnReady() {
    IntelCommandSetFlash::OnReady();
    PowerUp();
}

uint32_t Intel28F256K3::WriteBufferWords(uint32_t, uint32_t value) {
    return (value & kBufferCountMask) + 1u;
}

void Intel28F256K3::PowerUp() {
    ResetToReadArray();
    for (uint32_t i = 0; i < NumBlocks(); ++i) SetBlockLockBits(i, kLocked);
    rcr_ = kRcrReset;
}

bool Intel28F256K3::PartCommand(uint32_t addr, uint8_t cmd) {
    switch (cmd) {
        case 0x50u:
            ClearStatus();
            RestoreArray();
            return true;
        case 0xD0u:
            RestoreArray();
            return true;
        case 0xB0u:
            PresentStatus(addr);
            return true;
    }
    if (IsK3Command(cmd)) return false;
    RestoreArray();
    return true;
}

void Intel28F256K3::LockSetupCommand(uint32_t addr, uint8_t cmd) {
    const uint32_t block = BlockIndexChecked(addr);
    switch (cmd) {
        case 0x01u:
            SetBlockLockBits(block, uint8_t(BlockLockBits(block) | kLocked));
            break;
        case 0xD0u:
            if (BlockLockBits(block) & kLockedDown)
                HaltUnsupportedAccess("flash unlock of a locked-down block", addr, cmd);
            SetBlockLockBits(block, 0u);
            break;
        case 0x2Fu:
            SetBlockLockBits(block, kLocked | kLockedDown);
            break;
        case 0x03u:
            rcr_ = uint16_t((addr - MmioBase()) / BusBytes());
            break;
        default:
            IntelCommandSetFlash::LockSetupCommand(addr, cmd);
            return;
    }
    PresentStatus(addr);
}

void Intel28F256K3::PresentIdentifier(uint32_t) {
    RestoreArray();
    EmitLane(IdentifierWord(kIdManufacturer), Manufacturer());
    EmitLane(IdentifierWord(kIdDevice), Device());
    EmitLane(IdentifierWord(kIdConfiguration), rcr_);
    EmitLane(IdentifierWord(kIdPrLock0), kPrLock0FactoryProgrammed);
    for (uint32_t word = kIdFactoryPrFirst; word <= kIdOtpLast; ++word)
        EmitLane(IdentifierWord(word), word <= kIdFactoryPrLast ? kFactoryUniqueWord : kOtpErased);
    for (uint32_t i = 0; i < NumBlocks(); ++i)
        EmitLane(BlockBase(i) + kIdBlockStatus * BusBytes(), BlockLockBits(i));
}

void Intel28F256K3::SaveState(StateWriter& w) {
    IntelCommandSetFlash::SaveState(w);
    w.Write("rcr", rcr_);
}

void Intel28F256K3::RestoreState(StateReader& r) {
    IntelCommandSetFlash::RestoreState(r);
    r.Read("rcr", rcr_);
}

}
