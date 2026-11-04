#include "sharp_mobilon_hc_cmtt.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/log.h>
#include <cerfcore/boards/board_context.h>
#include "sharp_mobilon_hc_family_id.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kCmdInit  = 0x01200010u;
constexpr uint32_t kCmdRead  = 0x00002027u;
constexpr uint32_t kCmdWrite = 0x00000017u;

constexpr uint32_t kWriteReady = 0x00000200u;

constexpr uint32_t kDataStrobeLo = 0x97u;
constexpr uint32_t kDataStrobeHi = 0x9Fu;

}  /* namespace */

bool SharpMobilonHcCmtt::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::SharpMobilonHc;
}

void SharpMobilonHcCmtt::WriteDataPort(uint32_t value) {
    switch (value) {
        case kCmdInit:
        case kCmdRead:
            return;
        case kCmdWrite:
            write_ready_ = true;
            return;
        case 0u:
            write_ready_ = false;
            return;
        default:
            if ((value & 0xFFu) == kDataStrobeLo || (value & 0xFFu) == kDataStrobeHi) {
                write_ready_ = false;
                return;
            }
            emu_.Get<Fatal>().Die("Cmtt CS3+0x00 unmodeled command 0x%08X", value);
    }
}

uint32_t SharpMobilonHcCmtt::ReadDataPort() {
    return write_ready_ ? kWriteReady : 0u;
}

uint8_t SharpMobilonHcCmtt::ReadStatusByte() {
    return 0u;
}

bool SharpMobilonHcCmtt::ReadReady() const {
    return true;
}

REGISTER_SERVICE(SharpMobilonHcCmtt);

}
