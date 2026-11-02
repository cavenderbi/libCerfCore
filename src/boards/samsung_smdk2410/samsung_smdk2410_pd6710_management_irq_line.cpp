#include "../../peripherals/cirrus_pd6710/pd6710_management_irq_line.h"

#include <cerfcore/boards/board_context.h>
#include "samsung_smdk2410_family_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include "../../socs/irq_controller.h"
#include "../../socs/s3c2410/s3c2410_held_source_levels.h"

#include <atomic>

namespace cerfcore {

namespace {

constexpr int kEint3SourceBit = 3;

class SamsungSmdk2410Pd6710ManagementIrqLine : public Pd6710ManagementIrqLine,
                                                public S3C2410HeldLevelSource {
public:
    using Pd6710ManagementIrqLine::Pd6710ManagementIrqLine;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::SamsungSmdk2410;
    }

    void OnReady() override {
        emu_.Get<S3C2410HeldSourceLevels>().RegisterMain(kEint3SourceBit, this);
    }

    void Pulse() override {
        emu_.Get<IrqController>().AssertIrq(kEint3SourceBit);
    }

    void SetLevel(bool level) override {
        const bool was = level_.exchange(level, std::memory_order_acq_rel);
        if (level && !was)
            emu_.Get<IrqController>().AssertIrq(kEint3SourceBit);
    }

    bool LevelAsserted(int) override {
        return level_.load(std::memory_order_acquire);
    }

private:
    std::atomic<bool> level_{false};
};

}

REGISTER_SERVICE_AS(SamsungSmdk2410Pd6710ManagementIrqLine, Pd6710ManagementIrqLine);

}
