#include "../../socs/vr4101/vr4101_piu_inputs.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/battery.h>
#include <cerfcore/host/machine_object_tree.h>
#include "../nec_battery_adc_steps.h"
#include "nec_mobilepro_200_family_id.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint16_t kAinMainBattery = 0u;
constexpr uint16_t kAinXMinus      = 1u;
constexpr uint16_t kAinBackupCell  = 2u;
constexpr uint16_t kAinYMinus      = 3u;
constexpr uint16_t kAinAcAdapter   = 4u;
constexpr uint16_t kAinBatteryType = 5u;
constexpr uint16_t kTestMidScale   = 0x0Bu;
constexpr uint16_t kTestRefLow     = 0x0Cu;
constexpr uint16_t kTestRefHigh    = 0x0Du;

constexpr uint16_t kDriveYHigh   = 0x19u;
constexpr uint16_t kDriveYLow    = 0x0Bu;
constexpr uint16_t kDriveXHigh   = 0x1Au;
constexpr uint16_t kDriveXLow    = 0x13u;
constexpr uint16_t kPenDetect    = 0x1Eu;

constexpr uint16_t kFullScale       = 1023u;
constexpr uint16_t kMidScale        = 0x200u;
constexpr uint16_t kUnwiredInput    = 0u;
constexpr uint16_t kNoPlateContact  = 0u;
constexpr uint16_t kAcAbsent        = 0u;
constexpr uint16_t kAcPresent       = kFullScale;
constexpr uint16_t kBatteryTypeOne  = kFullScale;
constexpr uint16_t kBackupCellFull  = kFullScale;

constexpr NecBatteryAdcStep kMainPack[] = {
    {1023u, 100}, {889u, 90}, {858u, 80}, {834u, 70}, {818u, 60}, {806u, 50},
    {790u, 40},   {778u, 30}, {758u, 20}, {734u, 10}, {639u, 0},
};

class NecMobilepro200PiuInputs : public Vr4101PiuInputs {
public:
    using Vr4101PiuInputs::Vr4101PiuInputs;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::NecMobilepro200;
    }

    void OnReady() override { emu_.Get<MachineObjectTree>().Add(&battery_); }

    uint32_t ConverterBits() const override { return 10u; }

    uint16_t Convert(uint16_t ain, uint16_t pencont, const Vr41xxPiuPenPoint& pen) override {
        switch (ain) {
            case kAinMainBattery: return NecBatteryAdcWord(kMainPack, battery_.ChargePercent());
            case kAinBackupCell:  return kBackupCellFull;
            case kAinAcAdapter:   return battery_.OnExternalPower() ? kAcPresent : kAcAbsent;
            case kAinBatteryType: return kBatteryTypeOne;
            case kAinXMinus:      return XPlate(pencont, pen);
            case kAinYMinus:      return YPlate(pencont, pen);
            case kTestMidScale:   return kMidScale;
            case kTestRefLow:     return 0u;
            case kTestRefHigh:    return kFullScale;
            default:              return kUnwiredInput;
        }
    }

private:
    static uint16_t XPlate(uint16_t pencont, const Vr41xxPiuPenPoint& pen) {
        if (!pen.down) return kNoPlateContact;
        if (pencont == kDriveYHigh) return pen.y;
        if (pencont == kDriveYLow) return static_cast<uint16_t>(kFullScale - pen.y);
        return kNoPlateContact;
    }

    static uint16_t YPlate(uint16_t pencont, const Vr41xxPiuPenPoint& pen) {
        if (pencont == kPenDetect) return pen.down ? 0u : kFullScale;
        if (!pen.down) return kNoPlateContact;
        if (pencont == kDriveXHigh) return pen.x;
        if (pencont == kDriveXLow) return static_cast<uint16_t>(kFullScale - pen.x);
        return kNoPlateContact;
    }

    Battery battery_{emu_};
};

}

REGISTER_SERVICE_AS_ALSO(NecMobilepro200PiuInputs, Vr41xxPiuScreen, Vr4101PiuInputs);

}
