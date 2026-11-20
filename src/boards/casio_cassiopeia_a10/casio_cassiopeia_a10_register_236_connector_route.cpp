#include "casio_cassiopeia_a10_connector_route.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_members.h"
#include "casio_cassiopeia_a10_register_236.h"
#include "casio_cassiopeia_a10_serial_control.h"

#include <cstdint>
#include <utility>

namespace cerfcore {

namespace {

constexpr uint16_t kRouteBits      = 0x0003u;
constexpr uint16_t kScifRouteCable = 0x0002u;
constexpr uint16_t kRouteSci       = 0x0001u;

class CasioCassiopeiaA10Register236ConnectorRoute : public CasioCassiopeiaA10ConnectorRoute {
public:
    using CasioCassiopeiaA10ConnectorRoute::CasioCassiopeiaA10ConnectorRoute;

    bool ShouldRegister() override {
        BoardContext& board = emu_.Get<BoardContext>();
        return board.GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10 &&
               CasioCassiopeiaA10IsA51Class(board.GetBoardId());
    }

    void OnReady() override {
        control_   = &emu_.Get<CasioCassiopeiaA10SerialControl>();
        sci_route_ = &emu_.Get<CasioCassiopeiaA10Register236>();
    }

    std::optional<CasioCassiopeiaA10SerialUnit> Routed() const override {
        if ((control_->Value() & kRouteBits) == kScifRouteCable) return CasioCassiopeiaA10SerialUnit::kScif;
        if ((sci_route_->Value() & kRouteSci) != 0u) return CasioCassiopeiaA10SerialUnit::kSci;
        return std::nullopt;
    }

    std::optional<CasioCassiopeiaA10SerialUnit> Joined() const override {
        if (Routed() == CasioCassiopeiaA10SerialUnit::kScif) return CasioCassiopeiaA10SerialUnit::kSci;
        return std::nullopt;
    }

    void AddChangeListener(CallbackList<>::Callback listener) override {
        control_->AddChangeListener(listener);
        sci_route_->AddChangeListener(std::move(listener));
    }

private:
    CasioCassiopeiaA10SerialControl* control_   = nullptr;
    CasioCassiopeiaA10Register236*   sci_route_ = nullptr;
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10Register236ConnectorRoute, CasioCassiopeiaA10ConnectorRoute);

}
