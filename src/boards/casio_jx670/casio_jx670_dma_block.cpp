#include "casio_jx670_dma_block.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/cpu/emulated_memory.h>
#include <cerfengine/mips/mips_mmu.h>
#include "casio_jx670_family_id.h"

namespace cerfcore {

namespace {

constexpr uint32_t kCaptureBlockBytes = 1000u;
constexpr uint32_t kBandBytes         = 1024u;
constexpr uint32_t kBandTwoFromHz     = 15001u;
constexpr uint32_t kBandFourFromHz    = 26001u;

}

bool CasioJx670DmaBlock::ShouldRegister() {
    return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioJx670;
}

uint32_t CasioJx670DmaBlock::GuestBlockBytes(uint32_t rate_hz, uint16_t channels,
                                             bool capture) const {
    if (capture) return kCaptureBlockBytes;
    const uint32_t band = rate_hz < kBandTwoFromHz ? 1u : rate_hz < kBandFourFromHz ? 2u : 4u;
    return band * kBandBytes * channels;
}

uint32_t CasioJx670DmaBlock::RequireBlock(uint32_t start_va, uint32_t end_va, uint32_t rate_hz,
                                          uint16_t channels, bool capture) const {
    const uint32_t block_bytes = GuestBlockBytes(rate_hz, channels, capture);
    const bool kseg1 = start_va >= cerfengine::MipsSeg::kKseg1Base &&
                       end_va < cerfengine::MipsSeg::kKseg2Base && end_va >= start_va;
    const uint32_t length = end_va - start_va + 1u;
    if (!kseg1 || length != block_bytes) {
        emu_.Get<Fatal>().Die("EM-500 audio DMA descriptor 0x%08X..0x%08X is not a %u-byte "
                              "kseg1 block", start_va, end_va, block_bytes);
    }
    if (!emu_.Get<EmulatedMemory>().CanCopyRange(cerfengine::MipsSeg::UnmappedPa(start_va),
                                                 length, capture)) {
        emu_.Get<Fatal>().Die("EM-500 audio DMA descriptor 0x%08X..0x%08X is not one %s "
                              "buffer in memory", start_va, end_va,
                              capture ? "writable" : "readable");
    }
    return length;
}

REGISTER_SERVICE(CasioJx670DmaBlock);

}
