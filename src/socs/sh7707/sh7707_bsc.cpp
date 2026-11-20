#include "../sh77xx/sh77xx_bsc_impl.h"
#include "sh7707_id.h"

namespace cerfcore {

namespace {

constexpr sh77xx_bsc::Model kSh7707Bsc{
    .bcr1_writable               = 0x17FFu,
    .wcr1_writable               = 0x3FF3u,
    .mcr_writable                = 0xF37Fu,
    .pcr_writable                = 0x00FFu,
    .pcr_write_zero              = 0x0000u,
    .tpc_high_codes_reserved     = true,
    .tpc2_high_codes_reserved    = true,
    .sdram                       = false,
    .dramtp_area2_word           = 0x20u,
    .dramtp_area3_word           = 0x20u,
    .area2_dram_limits_all_areas = true,
    .mcr_dram_size_as_area3      = 0x0020u,
};

constexpr sh77xx_bsc::RegWindowModel kSh7707BscWindow = sh77xx_bsc::WindowOf(kSh7707Bsc);

class Sh7707Bsc : public sh77xx_bsc::Sh77xxBscBase<SocId::Sh7707, kSh7707BscWindow, kSh7707Bsc> {
public:
    using Sh77xxBscBase::Sh77xxBscBase;
};

}

REGISTER_SERVICE_AS(Sh7707Bsc, Sh77xxBsc);

}
