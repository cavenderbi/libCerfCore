#include "../vr41xx/vr41xx_bcu_impl.h"
#include "vr4101_id.h"
#include "vr4101_rstout_receiver.h"

#include <cstdint>

namespace cerfcore {

namespace {

using cerf_reg_window_detail::ReadKind;
using cerf_reg_window_detail::RegWindowModel;
using cerf_reg_window_detail::WriteKind;
using cerf_vr41xx_bcu_detail::Vr41xxBcuWindow;

constexpr uint32_t kCntIndex = 0u;
constexpr uint16_t kRstout   = 0x0001u;

constexpr RegWindowModel kModel = {
    0x0B000000u,
    0x20u,
    9u,
    false,
    {
        { ReadKind::kStored, WriteKind::kStored, 0x00E9u, 0x0000u, 0xFB06u },
        {},
        {},
        {},
        {},
        { ReadKind::kStored, WriteKind::kStored, 0x3377u, 0x0010u, 0xCC88u },
        {},
        {},
        {},
    },
};

class Vr4101Bcu : public Vr41xxBcuWindow<SocId::Vr4101, kModel> {
public:
    using Vr41xxBcuWindow::Vr41xxBcuWindow;

    void RestoreState(StateReader& r) override {
        Vr41xxBcuWindow::RestoreState(r);
        rstout_high_ = (StoredReg(kCntIndex) & kRstout) != 0u;
    }

protected:
    const char* EncodingRejected(uint32_t off, uint16_t value) const override {
        return vr41xx_bcu::SpeedWriteRfuEncoding(off, value);
    }

    bool LcdSpaceInverted() const override { return true; }

    void AfterStore(uint32_t i) override {
        if (i == kCntIndex) PublishRstout();
    }

    void AfterReset() override {
        Vr41xxBcuWindow::AfterReset();
        PublishRstout();
    }

private:
    void PublishRstout() {
        const bool high = (StoredReg(kCntIndex) & kRstout) != 0u;
        if (high == rstout_high_) return;
        rstout_high_ = high;
        emu_.Get<Vr4101RstoutReceiver>().RstoutChanged(high);
    }

    bool rstout_high_ = false;
};

}

REGISTER_SERVICE(Vr4101Bcu);

}
