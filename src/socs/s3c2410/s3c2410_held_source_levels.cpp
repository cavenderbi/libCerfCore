#include "s3c2410_held_source_levels.h"

#include <cerfcore/boards/board_context.h>
#include "s3c2410_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include "../irq_controller.h"

namespace cerfcore {

REGISTER_SERVICE(S3C2410HeldSourceLevels);

bool S3C2410HeldSourceLevels::ShouldRegister() {
    return emu_.Get<BoardContext>().GetSocId() == SocId::S3c2410;
}

void S3C2410HeldSourceLevels::RegisterMain(int main_source_bit, S3C2410HeldLevelSource* source) {
    Add(Entry{main_source_bit, kNoSubSource, source});
}

void S3C2410HeldSourceLevels::RegisterSub(int main_source_bit, int sub_source_bit,
                                          S3C2410HeldLevelSource* source) {
    Add(Entry{main_source_bit, sub_source_bit, source});
}

void S3C2410HeldSourceLevels::Add(const Entry& entry) {
    std::lock_guard<std::mutex> lk(mutex_);
    entries_.push_back(entry);
    if (entry.sub_bit == kNoSubSource)
        main_mask_.fetch_or(1u << entry.main_bit, std::memory_order_release);
    else
        sub_mask_.fetch_or(1u << entry.sub_bit, std::memory_order_release);
}

void S3C2410HeldSourceLevels::ReassertStillHeld(uint32_t cleared_main, uint32_t cleared_sub) {
    const bool main_hit = (cleared_main & main_mask_.load(std::memory_order_acquire)) != 0u;
    const bool sub_hit = (cleared_sub & sub_mask_.load(std::memory_order_acquire)) != 0u;
    if (!main_hit && !sub_hit)
        return;

    std::vector<Entry> snapshot;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        snapshot = entries_;
    }

    auto& intc = emu_.Get<IrqController>();
    for (const Entry& e : snapshot) {
        if (e.sub_bit == kNoSubSource) {
            if ((cleared_main & (1u << e.main_bit)) != 0u && e.source->LevelAsserted(e.main_bit))
                intc.AssertIrq(e.main_bit);
        } else if ((cleared_sub & (1u << e.sub_bit)) != 0u && e.source->LevelAsserted(e.sub_bit)) {
            intc.AssertSubIrq(e.main_bit, e.sub_bit);
        }
    }
}

}
