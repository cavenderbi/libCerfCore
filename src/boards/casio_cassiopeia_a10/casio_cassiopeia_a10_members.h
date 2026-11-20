#pragma once

#include "casio_cassiopeia_a20_id.h"
#include "casio_cassiopeia_a20f_id.h"
#include "casio_cassiopeia_a20j_id.h"
#include "casio_cassiopeia_a21s_id.h"
#include "casio_cassiopeia_a22t_id.h"
#include "casio_cassiopeia_a23g_id.h"
#include "casio_cassiopeia_a50_id.h"
#include "casio_cassiopeia_a51_id.h"
#include "casio_cassiopeia_a51v_id.h"
#include "casio_cassiopeia_a55v_id.h"
#include "casio_cassiopeia_a60_id.h"
#include "casio_cassiopeia_pa2400_id.h"
#include "casio_cassiopeia_pa2400w_id.h"
#include "casio_cassiopeia_pa_2000_id.h"
#include "casio_cassiopeia_pa_2000v_id.h"
#include "casio_cassiopeia_pa_2100_id.h"
#include "casio_cassiopeia_pa_2300v_id.h"

#include <algorithm>
#include <iterator>
#include <string_view>

namespace cerfcore {

inline constexpr std::string_view kCasioCassiopeiaA10A51Class[] = {
    BoardId::CasioCassiopeiaA50,    BoardId::CasioCassiopeiaA51,    BoardId::CasioCassiopeiaPa2000,
    BoardId::CasioCassiopeiaPa2100, BoardId::CasioCassiopeiaA51v,   BoardId::CasioCassiopeiaPa2000v,
    BoardId::CasioCassiopeiaPa2300v, BoardId::CasioCassiopeiaA55v,  BoardId::CasioCassiopeiaA60,
    BoardId::CasioCassiopeiaA20,    BoardId::CasioCassiopeiaA20j,   BoardId::CasioCassiopeiaA20f,
    BoardId::CasioCassiopeiaA21s,   BoardId::CasioCassiopeiaA22t,   BoardId::CasioCassiopeiaA23g,
    BoardId::CasioCassiopeiaPa2400, BoardId::CasioCassiopeiaPa2400w,
};

inline constexpr std::string_view kCasioCassiopeiaA10NoKeyboard[] = {
    BoardId::CasioCassiopeiaPa2100,
    BoardId::CasioCassiopeiaPa2300v,
    BoardId::CasioCassiopeiaPa2400,
    BoardId::CasioCassiopeiaPa2400w,
};

inline bool CasioCassiopeiaA10IsA51Class(std::string_view board_id) {
    return std::ranges::find(kCasioCassiopeiaA10A51Class, board_id) != std::ranges::end(kCasioCassiopeiaA10A51Class);
}

inline bool CasioCassiopeiaA10HasKeyboard(std::string_view board_id) {
    return std::ranges::find(kCasioCassiopeiaA10NoKeyboard, board_id) == std::ranges::end(kCasioCassiopeiaA10NoKeyboard);
}

}
