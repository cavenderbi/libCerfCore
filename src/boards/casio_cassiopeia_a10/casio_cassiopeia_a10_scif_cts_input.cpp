#include "../../socs/hd6417096/hd6417096_scif_cts_input.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_serial_connector.h"

#include <utility>

namespace cerfcore {

namespace {

class CasioCassiopeiaA10ScifCtsInput : public Hd6417096ScifCtsInput {
public:
    using Hd6417096ScifCtsInput::Hd6417096ScifCtsInput;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10;
    }

    void OnReady() override { connector_ = &emu_.Get<CasioCassiopeiaA10SerialConnector>(); }

    bool High() const override { return !connector_->Cts(); }

    void AddChangeListener(CallbackList<>::Callback listener) override {
        connector_->AddCtsListener(std::move(listener));
    }

private:
    CasioCassiopeiaA10SerialConnector* connector_ = nullptr;
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10ScifCtsInput, Hd6417096ScifCtsInput);

}
