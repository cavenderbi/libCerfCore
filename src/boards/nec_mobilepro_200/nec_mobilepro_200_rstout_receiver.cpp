#include "../../socs/vr4101/vr4101_rstout_receiver.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../peripherals/intel_82365sl/intel_82365sl.h"
#include "nec_mobilepro_200_family_id.h"

namespace cerfcore {

namespace {

class NecMobilepro200RstoutReceiver : public Vr4101RstoutReceiver {
public:
    using Vr4101RstoutReceiver::Vr4101RstoutReceiver;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::NecMobilepro200;
    }

    void RstoutChanged(bool high) override { emu_.Get<Intel82365sl>().DriveResetDrive(high); }
};

}

REGISTER_SERVICE_AS(NecMobilepro200RstoutReceiver, Vr4101RstoutReceiver);

}
