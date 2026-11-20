#include "guest_xip_image.h"

#include <cerfcore/boot/guest_cold_boot.h>
#include <cerfcore/core/byte_order.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/cpu/emulated_memory.h>
#include <cerfcore/socs/guest_cpu_reset.h>

#include <vector>

namespace cerfcore {

void GuestXipImage::PatchWord(uint32_t pa, uint32_t value) {
    uint8_t bytes[4];
    cerf::le::Put32(bytes, value);
    Patch(pa, bytes);
}

void GuestXipImage::PatchRam(uint32_t pa, std::span<const uint8_t> bytes) {
    emu_.Get<EmulatedMemory>().CopyIn(pa, bytes.data(), bytes.size());
    emu_.Get<GuestColdBoot>().RecordPatch(pa, uint32_t(bytes.size()));
}

void GuestXipImage::RestoreOnEveryReset(uint32_t pa, uint32_t length) {
    std::vector<uint8_t> initial(length);
    emu_.Get<EmulatedMemory>().CopyOut(pa, initial.data(), length);
    emu_.Get<GuestCpuReset>().RegisterResetListener(
        [this, pa, initial = std::move(initial)](ResetLineKind) {
            emu_.Get<EmulatedMemory>().CopyIn(pa, initial.data(), initial.size());
        });
}

}
