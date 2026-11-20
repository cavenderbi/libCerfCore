#include "intel_28f256k3.h"

#include <cerfcore/boards/board_context.h>
#include "../../boards/symbol_hamptons/symbol_hamptons_family_id.h"
#include "../../core/no_emulation_runtime_service.h"
#include "../../socs/pxa27x/pxa27x_power_manager.h"
#include "../../storage/nor_flash_store.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/device_config.h>
#include <cerfcore/core/fatal.h>

namespace cerfcore {

namespace {

class Intel28F256K3Mk500Cs0 : public Intel28F256K3 {
public:
    using Intel28F256K3::Intel28F256K3;

    bool ShouldRegister() override {
        auto* bd = emu_.TryGet<BoardContext>();
        return bd && bd->GetBoardFamilyId() == BoardFamilyId::SymbolHamptons;
    }

    void OnReady() override {
        Intel28F256K3::OnReady();
        emu_.Get<NoEmulationRuntimeService>().EnsureEmulationPrevented();
        if (!emu_.TryGet<NorFlashStore>()) {
            emu_.Get<Fatal>().UserError("NOR flash image '%s' (storage.nor) is missing or empty",
                                        emu_.Get<DeviceConfig>().storage_nor.c_str());
        }
    }

    void SaveState(StateWriter& w) override {
        emu_.Get<NorFlashStore>().SaveState(w);
        Intel28F256K3::SaveState(w);
    }

    void RestoreState(StateReader& r) override {
        RestoreArray();
        emu_.Get<NorFlashStore>().RestoreState(r);
        Intel28F256K3::RestoreState(r);
    }

    uint32_t MmioBase() const override { return 0x00000000u; }
    uint32_t MmioSize() const override { return 0x04000000u; }

protected:
    uint32_t Parallel() const override { return 2u; }
    bool     ArrayIsStorage() const override { return true; }

    bool ResetPinAsserted(ResetLineKind kind) const override {
        return emu_.Get<Pxa27xPowerManager>().ResetOutAsserted(kind);
    }

    void ArrayWillChange(uint32_t addr, uint32_t length) override {
        emu_.Get<NorFlashStore>().DropOverlay(addr - MmioBase(), length);
    }

    void ArrayStored(uint32_t addr, uint32_t length) override {
        emu_.Get<NorFlashStore>().Store(addr - MmioBase(), Host(addr), length);
    }
};

}

REGISTER_SERVICE(Intel28F256K3Mk500Cs0);

}
