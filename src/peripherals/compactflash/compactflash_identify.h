#pragma once

#include "compactflash_task_file.h"

#include <array>
#include <cstdint>

namespace cerfcore {

void BuildCompactFlashIdentify(std::array<uint8_t, 512>& block, const CompactFlashTaskFile& task_file,
                               uint64_t total_sectors, bool multiple_enabled);

}
