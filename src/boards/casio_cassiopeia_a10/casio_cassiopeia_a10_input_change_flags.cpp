#include "casio_cassiopeia_a10_chip_register.h"

#include "casio_cassiopeia_a10_battery.h"
#include "casio_cassiopeia_a10_chip_intc.h"
#include "casio_cassiopeia_a10_input_change_enables.h"
#include "casio_cassiopeia_a10_pc_card.h"
#include "casio_cassiopeia_a10_slot_inputs.h"

#include <cstdint>
#include <mutex>

namespace cerfcore {

namespace {

constexpr uint32_t kInputChangeFlags = 0x10000264u;

class CasioCassiopeiaA10InputChangeFlags
    : public CasioCassiopeiaA10FlagsChipRegister<kInputChangeFlags, kCasioCassiopeiaA10InputChangeBits> {
public:
    using CasioCassiopeiaA10FlagsChipRegister::CasioCassiopeiaA10FlagsChipRegister;

    void OnReady() override {
        inputs_    = &emu_.Get<CasioCassiopeiaA10SlotInputs>();
        enables_   = &emu_.Get<CasioCassiopeiaA10InputChangeEnables>();
        chip_intc_ = &emu_.Get<CasioCassiopeiaA10ChipIntc>();
        CasioCassiopeiaA10FlagsChipRegister::OnReady();
        enables_->AddChangeListener([this] { DriveRequest(); });
        AddChangeListener([this] { DriveRequest(); });
        emu_.Get<CasioCassiopeiaA10PcCard>().AddCardDetectListener([this] { OnInputs(); });
        emu_.Get<CasioCassiopeiaA10Battery>().AddChangeListener([this] { OnInputs(); });
        emu_.Get<GuestCpuReset>().RegisterResetReleaseListener([this] { Resync(); });
        Resync();
    }

    void SaveState(StateWriter& w) override {
        CasioCassiopeiaA10FlagsChipRegister::SaveState(w);
        std::lock_guard<std::mutex> lk(mtx_);
        w.Write("last_inputs", last_inputs_);
    }

    void RestoreState(StateReader& r) override {
        CasioCassiopeiaA10FlagsChipRegister::RestoreState(r);
        std::lock_guard<std::mutex> lk(mtx_);
        r.Read("last_inputs", last_inputs_);
    }

    void PostRestore() override { DriveRequest(); }

private:
    void OnInputs() {
        std::lock_guard<std::mutex> lk(mtx_);
        const uint16_t now     = inputs_->Inputs();
        const uint16_t changed = static_cast<uint16_t>(now ^ last_inputs_);
        last_inputs_           = now;
        if (changed != 0u) Latch(changed);
    }

    void Resync() {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            last_inputs_ = inputs_->Inputs();
        }
        DriveRequest();
    }

    void DriveRequest() {
        const bool request = (enables_->Value() & Value() & kCasioCassiopeiaA10InputChangeBits) != 0u;
        chip_intc_->SetInputLevel(CasioCassiopeiaA10ChipInput::kSlotInputChange, request);
    }

    std::mutex                            mtx_;
    CasioCassiopeiaA10SlotInputs*         inputs_      = nullptr;
    CasioCassiopeiaA10InputChangeEnables* enables_     = nullptr;
    CasioCassiopeiaA10ChipIntc*           chip_intc_   = nullptr;
    uint16_t                              last_inputs_ = 0u;
};

}

REGISTER_SERVICE(CasioCassiopeiaA10InputChangeFlags);

}
