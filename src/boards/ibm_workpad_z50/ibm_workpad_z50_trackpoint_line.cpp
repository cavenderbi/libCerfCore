#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/service.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include "../../peripherals/ibm_trackpoint/ibm_trackpoint.h"
#include "../../socs/vr41xx/vr41xx_dsiu.h"
#include "../../socs/vr41xx/vr41xx_giu.h"
#include "ibm_workpad_z50_family_id.h"

#include <cstdint>
#include <optional>

namespace cerfcore {

namespace {

constexpr int kRtsGpio = 46;
constexpr int kDtrGpio = 47;

class IbmWorkpadZ50TrackpointLine : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::IbmWorkpadZ50;
    }

    void OnReady() override {
        giu_  = &emu_.Get<Vr41xxGiu>();
        dsiu_ = &emu_.Get<Vr41xxDsiu>();
        trackpoint_.emplace(
            [this] { return dsiu_->TransmitFormat(); },
            [this](uint8_t data, const SerialLine::LineConfig& format) {
                dsiu_->ReceiveFromLine(data, format);
            },
            [this](const char* what, unsigned value) {
                emu_.Get<Fatal>().Die("IBM TrackPoint: %s 0x%X", what, value);
            });
        trackpoint_->PowerOn();
        dsiu_->AttachEndpoint(&*trackpoint_);
        giu_->RegisterOutputDriveListener([this] {
            trackpoint_->SetLines(LineHigh(kDtrGpio), LineHigh(kRtsGpio));
        });
        auto& reset = emu_.Get<GuestCpuReset>();
        reset.RegisterResetListener([this, &reset](ResetLineKind kind) {
            if (kind == ResetLineKind::Rtc || reset.DeliveredResetWasResume()) trackpoint_->PowerOn();
        });
    }

private:
    bool LineHigh(int gpio) { return giu_->OutputDrive(gpio) != Vr41xxPinDrive::kLow; }

    Vr41xxGiu*                   giu_  = nullptr;
    Vr41xxDsiu*                  dsiu_ = nullptr;
    std::optional<IbmTrackpoint> trackpoint_;
};

}

REGISTER_SERVICE(IbmWorkpadZ50TrackpointLine);

}
