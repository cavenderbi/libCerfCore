#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/service.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include "../../socs/pr31x00/pr31x00_biu.h"
#include "../../socs/pr31x00/pr31x00_clock.h"
#include "../../socs/pr31x00/pr31x00_intc.h"
#include <cerfcore/boards/board_context.h>
#include "sharp_mobilon_hc_family_id.h"
#include "sharp_mobilon_hc_memory.h"

#include <cstdint>

namespace cerfcore {

namespace {

/* sharp_mobilon_hc4100_hpc2 serial.dll sub_1475B7C: UART CTL2 BAUDRATE = 0x38400 / baud - 1;
   Baud Rate = f_UARTCLK / ((BAUDRATE + 1) * 16) (TMPR3911 §16.5.2); UARTACLK 3.6864 MHz at
   CSERDIV 011 with a 9.216 MHz crystal (TMPR3911 Table 6.3.1, p.6-9). */
constexpr uint32_t kCserDiv = 3u;

class SharpMobilonHcBootloader : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::SharpMobilonHc;
    }

    void OnReady() override {
        Apply();
        emu_.Get<GuestCpuReset>().RegisterResetReleaseListener([this] { Apply(); });
    }

private:
    void Apply() {
        auto& intc = emu_.Get<Pr31x00Intc>();
        intc.DisableAllSources();
        intc.ClearAllPending();
        intc.SetGlobalEnable();
        emu_.Get<Pr31x00Clock>().PresetSerialClock(kCserDiv);
        emu_.Get<Pr31x00Biu>().PresetBanks(emu_.Get<SharpMobilonHcMemory>().MemConfig0Banks());
    }
};

}  /* namespace */

REGISTER_SERVICE(SharpMobilonHcBootloader);

}
