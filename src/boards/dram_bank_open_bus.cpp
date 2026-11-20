#include "dram_bank_open_bus.h"

#include <cerfcore/core/cerf_emulator.h>
#include "page_table_builder.h"

namespace cerfcore {

uint32_t DramBankOpenBus::BackedBytes() const {
    for (const BackedRegion& r : emu_.Get<PageTableBuilder>().BackedMemoryRegions()) {
        if (r.pa_base == BankPaBase()) return r.decode_span != 0u ? r.decode_span : r.size;
    }
    return 0u;
}

uint32_t DramBankOpenBus::MmioBase() const { return BankPaBase() + BackedBytes(); }

uint32_t DramBankOpenBus::MmioSize() const { return BankBytes() - BackedBytes(); }

bool DramBankOpenBus::WindowOnBoard(const BoardContext&) const { return BackedBytes() < BankBytes(); }

}
