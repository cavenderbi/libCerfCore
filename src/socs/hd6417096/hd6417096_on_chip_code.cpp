#include <cerfcore/core/service.h>

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/cpu/emulated_memory.h>
#include <cerfcore/cpu/memory_kind.h>
#include "hd6417096_id.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kRoutinePa = 0xE00001DEu;
constexpr uint8_t  kRoutine[] = {0x0Bu, 0x00u, 0x42u, 0x25u};

class Hd6417096OnChipCode : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override { return emu_.Get<BoardContext>().GetSocId() == SocId::Hd6417096; }

    void OnReady() override {
        EmulatedMemory& memory = emu_.Get<EmulatedMemory>();
        memory.AddRegion(kRoutinePa, sizeof(kRoutine), MemoryKind::kRom);
        memory.LoadContents(kRoutinePa, kRoutine, sizeof(kRoutine));
    }
};

}

REGISTER_SERVICE(Hd6417096OnChipCode);

}
