#include "samsung_smdk2410_device_emulator_first_gen_boards.h"

#include "devemu_gen1_id.h"
#include "devemu_gen1_sp_id.h"
#include "devemu_gen1_sp_qvga_id.h"
#include "devemu_gen1_vga_id.h"
#include "devemu_gen1_wince_svga_id.h"

namespace cerfcore {

bool IsDeviceEmulatorFirstGenBoard(std::string_view board_id) {
    return board_id == BoardId::DevemuGen1 || board_id == BoardId::DevemuGen1Vga ||
           board_id == BoardId::DevemuGen1Sp || board_id == BoardId::DevemuGen1SpQvga ||
           board_id == BoardId::DevemuGen1WinceSvga;
}

}
