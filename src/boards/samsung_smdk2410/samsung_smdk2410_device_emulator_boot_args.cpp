#include "samsung_smdk2410_device_emulator_boot_args.h"

#include <cerfcore/boards/board_context.h>
#include "samsung_smdk2410_family_id.h"

#include <cerfcore/boot/guest_cold_boot.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/device_config.h>
#include <cerfcore/core/log.h>
#include <cerfcore/cpu/emulated_memory.h>

namespace cerfcore {

namespace {

constexpr uint16_t kDefaultColorDepthBpp = 16u;
constexpr uint8_t  kSoftReset            = 1u;

}

bool SamsungSmdk2410DeviceEmulatorBootArgs::ShouldRegister() {
    auto* board = emu_.TryGet<BoardContext>();
    if (!board || board->GetBoardFamilyId() != BoardFamilyId::SamsungSmdk2410) return false;
    if (emu_.Get<DeviceConfig>().guest_additions) return false;
    return MatchesBoard(board->GetBoardId());
}

void SamsungSmdk2410DeviceEmulatorBootArgs::OnReady() {
    WriteBlocks();
    emu_.Get<GuestColdBoot>().RegisterReplay([this] { WriteBlocks(); });
}

void SamsungSmdk2410DeviceEmulatorBootArgs::WriteBlocks() {
    auto& cfg = emu_.Get<DeviceConfig>();
    auto& mem = emu_.Get<EmulatedMemory>();

    const uint16_t w   = static_cast<uint16_t>(cfg.board_configurable_screen_width);
    const uint16_t h   = static_cast<uint16_t>(cfg.board_configurable_screen_height);
    const uint16_t bpp = cfg.board_configurable_screen_bpp != 0u
                       ? static_cast<uint16_t>(cfg.board_configurable_screen_bpp)
                       : kDefaultColorDepthBpp;

    for (const SamsungSmdk2410DeviceEmulatorArgsBlock& block : Blocks()) {
        mem.WriteWord(block.signature_pa, block.signature);
        mem.WriteHalf(block.width_pa, w);
        mem.WriteHalf(block.height_pa, h);
        mem.WriteHalf(block.bits_per_pixel_pa, bpp);
        if (block.has_soft_reset_flag) mem.WriteByte(block.soft_reset_pa, kSoftReset);

        LOG(Board, "SamsungSmdk2410DeviceEmulatorBootArgs: signature 0x%08X at PA 0x%08X, "
                   "Width=%u Height=%u BitsPerPixel=%u\n",
            block.signature, block.signature_pa, (unsigned)w, (unsigned)h, (unsigned)bpp);
    }
}

}
