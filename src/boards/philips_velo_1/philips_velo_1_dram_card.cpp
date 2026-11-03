#include "../../socs/pr31x00/pr31x00_mfio_slave.h"

#include <cerfcore/core/cerf_emulator.h>
#include "../../peripherals/i2c/i2c_bit_bang_target.h"
#include "../../socs/pr31x00/pr31x00_io.h"
#include <cerfcore/boards/board_context.h>
#include "philips_velo_1_dram_card_id_eeprom.h"
#include "philips_velo_1_dram_card_profile.h"
#include "philips_velo_1_family_id.h"

#include <cstdint>
#include <optional>

namespace cerfcore {

namespace {

constexpr uint32_t kSclPin = 18;
constexpr uint32_t kSdaPin = 20;
constexpr uint32_t kScl    = 1u << kSclPin;
constexpr uint32_t kSda    = 1u << kSdaPin;

class PhilipsVelo1DramCard : public Pr31x00MfioSlave {
public:
    using Pr31x00MfioSlave::Pr31x00MfioSlave;

    bool ShouldRegister() override {
        auto* bd = emu_.TryGet<BoardContext>();
        return bd && bd->GetBoardFamilyId() == BoardFamilyId::PhilipsVelo1 &&
               FindPhilipsVelo1DramCard(bd->GetBoardId()) != nullptr;
    }

    void OnReady() override {
        eeprom_ = &emu_.Get<PhilipsVelo1DramCardIdEeprom>();
        target_.emplace([this](bool released) { DriveSda(released); },
                        std::vector<I2cSlave*>{ eeprom_ }, I2cBitBangTarget::Unmatched{});
        DriveSda(true);
    }

    void OnMfioOut(uint32_t mfio_dout, uint32_t out_mask) override {
        const bool scl = (out_mask & kScl) == 0u || (mfio_dout & kScl) != 0u;
        const bool sda = (out_mask & kSda) == 0u || (mfio_dout & kSda) != 0u;
        target_->OnLines(scl, sda);
    }

    void SaveState(StateWriter& w) override {
        target_->SaveState(w);
        eeprom_->SaveState(w);
    }

    void RestoreState(StateReader& r) override {
        target_->RestoreState(r);
        eeprom_->RestoreState(r);
    }

private:
    void DriveSda(bool level) {
        emu_.Get<Pr31x00Io>().DriveMfioInput(kSdaPin, level);
    }

    PhilipsVelo1DramCardIdEeprom*   eeprom_ = nullptr;
    std::optional<I2cBitBangTarget> target_;
};

}

REGISTER_SERVICE_AS(PhilipsVelo1DramCard, Pr31x00MfioSlave);

}
