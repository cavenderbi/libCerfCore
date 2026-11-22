#pragma once

#include <cerfcore/core/service.h>
#include "../../socs/sh77xx/sh77xx_intc.h"
#include "hp_600_keyboard_layout.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <span>

namespace cerfcore {

class Sh77xxPortKeyMatrix;

enum class Hp600InputNet : uint8_t {
    kAcAdapterOff,
    kSerialCtsOff,
    kNotificationLedLoop,
    kPlayButtonUp,
    kRewindButtonUp,
    kRecordButtonUp,
    kCount,
};

inline constexpr uint32_t kHp600InputNetCodeBase = kHp600KeyCells;

class Hp600InputNets : public Service {
public:
    using Service::Service;

    void OnReady() override;

    void    Drive(Hp600InputNet net, bool high);
    bool    Wired(Hp600InputNet net) const;
    uint8_t WiredInputs(uint32_t port) const;
    uint8_t NetLevels(uint32_t port, uint8_t nets) const;

protected:
    static constexpr uint32_t kNetCount = static_cast<uint32_t>(Hp600InputNet::kCount);

    struct Landing {
        enum class Kind : uint8_t { kNone, kIntcPin, kPortPin };
        Kind          kind = Kind::kNone;
        Sh77xxIntcPin intc = Sh77xxIntcPin::kIrq0;
        uint32_t      port = 0u;
        uint8_t       mask = 0u;
    };

    virtual std::span<const Landing, kNetCount> Landings() const = 0;
    virtual uint8_t UndrivenInputs(uint32_t port) const = 0;
    virtual uint16_t ButtonSummaryColumns() const = 0;

private:
    uint32_t Apply(Hp600InputNet net, bool high);
    void     ReleaseButtons();

    Sh77xxIntc*           intc_   = nullptr;
    Sh77xxPortKeyMatrix*  matrix_ = nullptr;
    std::mutex            buttons_mtx_;
    std::atomic<uint32_t> high_{(1u << kNetCount) - 1u};
};

}
