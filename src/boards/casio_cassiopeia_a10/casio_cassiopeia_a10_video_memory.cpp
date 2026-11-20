#include "casio_cassiopeia_a10_video_memory.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/byte_order.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/cpu/emulated_memory.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../../peripherals/peripheral_dispatcher.h"
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_members.h"

namespace cerfcore {

REGISTER_SERVICE(CasioCassiopeiaA10VideoMemory);

namespace {

constexpr uint32_t kFramePa      = 0x08000000u;
constexpr uint32_t kFrameBytes   = 0xF000u;
constexpr uint32_t kControllerPa = 0x0801FF00u;

}

bool CasioCassiopeiaA10VideoMemory::ShouldRegister() {
    BoardContext& board = emu_.Get<BoardContext>();
    return board.GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10 &&
           CasioCassiopeiaA10IsA51Class(board.GetBoardId());
}

void CasioCassiopeiaA10VideoMemory::OnReady() {
    EmulatedMemory& memory = emu_.Get<EmulatedMemory>();
    memory.AddRegion(kFramePa, kFrameBytes, MemoryKind::kRam);
    frame_ = memory.TryTranslate(kFramePa);
    emu_.Get<PeripheralDispatcher>().Register(this);
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) { controller_.fill(0u); });
}

uint32_t CasioCassiopeiaA10VideoMemory::MmioBase() const { return kControllerPa; }
uint32_t CasioCassiopeiaA10VideoMemory::MmioSize() const { return kControllerBytes; }

uint8_t CasioCassiopeiaA10VideoMemory::ReadByte(uint32_t addr) { return controller_[addr - kControllerPa]; }

uint16_t CasioCassiopeiaA10VideoMemory::ReadHalf(uint32_t addr) {
    return cerf::le::U16(controller_.data(), addr - kControllerPa);
}

uint32_t CasioCassiopeiaA10VideoMemory::ReadWord(uint32_t addr) {
    return cerf::le::U32(controller_.data(), addr - kControllerPa);
}

void CasioCassiopeiaA10VideoMemory::WriteByte(uint32_t addr, uint8_t value) { controller_[addr - kControllerPa] = value; }

void CasioCassiopeiaA10VideoMemory::WriteHalf(uint32_t addr, uint16_t value) {
    cerf::le::Put16(controller_.data() + (addr - kControllerPa), value);
}

void CasioCassiopeiaA10VideoMemory::WriteWord(uint32_t addr, uint32_t value) {
    cerf::le::Put32(controller_.data() + (addr - kControllerPa), value);
}

void CasioCassiopeiaA10VideoMemory::SaveState(StateWriter& w) { w.Write("controller", controller_); }
void CasioCassiopeiaA10VideoMemory::RestoreState(StateReader& r) { r.Read("controller", controller_); }

const uint8_t* CasioCassiopeiaA10VideoMemory::Frame() const { return frame_; }

}
