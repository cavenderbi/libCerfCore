#include "casio_cassiopeia_a10_chip_register.h"

#include "casio_cassiopeia_a10_chip_intc.h"
#include "casio_cassiopeia_a10_nmi_line.h"
#include "casio_cassiopeia_a10_wake_enables.h"

#include <cstdint>
#include <mutex>

namespace cerfcore {

namespace {

constexpr uint32_t kLatches = 0x10000002u;

class CasioCassiopeiaA10WakeLatch : public CasioCassiopeiaA10ChipWord<kLatches> {
public:
    using CasioCassiopeiaA10ChipWord::CasioCassiopeiaA10ChipWord;

    void OnReady() override {
        nmi_line_ = &emu_.Get<CasioCassiopeiaA10NmiLine>();
        enables_  = &emu_.Get<CasioCassiopeiaA10WakeEnables>();
        enables_->AddChangeListener([this] {
            std::lock_guard<std::mutex> lk(mtx_);
            DriveNmiLocked();
        });
        emu_.Get<CasioCassiopeiaA10ChipIntc>().AddRisingEdgeListener([this](uint16_t bits) { Latch(bits); });
        emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) { Reset(); });
        CasioCassiopeiaA10ChipWord::OnReady();
        std::lock_guard<std::mutex> lk(mtx_);
        DriveNmiLocked();
    }

    uint16_t ReadHalf(uint32_t) override {
        std::lock_guard<std::mutex> lk(mtx_);
        return latches_;
    }

    void WriteHalf(uint32_t, uint16_t value) override {
        std::lock_guard<std::mutex> lk(mtx_);
        latches_ = static_cast<uint16_t>(latches_ & ~value);
        DriveNmiLocked();
    }

    void SaveState(StateWriter& w) override {
        std::lock_guard<std::mutex> lk(mtx_);
        w.Write("latches", latches_);
    }

    void RestoreState(StateReader& r) override {
        std::lock_guard<std::mutex> lk(mtx_);
        r.Read("latches", latches_);
    }

    void PostRestore() override {
        std::lock_guard<std::mutex> lk(mtx_);
        DriveNmiLocked();
    }

private:
    void Latch(uint16_t bits) {
        std::lock_guard<std::mutex> lk(mtx_);
        latches_ = static_cast<uint16_t>(latches_ | (bits & enables_->Value()));
        DriveNmiLocked();
    }

    void DriveNmiLocked() { nmi_line_->SetWakeRequest((latches_ & enables_->Value()) != 0u); }

    void Reset() {
        std::lock_guard<std::mutex> lk(mtx_);
        latches_ = 0u;
        DriveNmiLocked();
    }

    std::mutex                     mtx_;
    CasioCassiopeiaA10NmiLine*     nmi_line_ = nullptr;
    CasioCassiopeiaA10WakeEnables* enables_  = nullptr;
    uint16_t                       latches_  = 0u;
};

}

REGISTER_SERVICE(CasioCassiopeiaA10WakeLatch);

}
