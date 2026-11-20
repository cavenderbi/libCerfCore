#pragma once

#include "casio_cassiopeia_a10_chip_register.h"

#include <cerfcore/core/callback_list.h>
#include "../../peripherals/serial/serial_line_switch.h"

#include <atomic>
#include <cstdint>
#include <optional>

namespace cerfcore {

inline constexpr uint32_t kCasioCassiopeiaA10SerialLines    = 0x10000234u;
inline constexpr uint16_t kCasioCassiopeiaA10LineOutputs    = 0x0038u;
inline constexpr uint16_t kCasioCassiopeiaA10LineCarrier    = 0x0004u;
inline constexpr uint16_t kCasioCassiopeiaA10LineDtr        = 0x0008u;
inline constexpr uint16_t kCasioCassiopeiaA10LineScifToCable = 0x0010u;

enum class CasioCassiopeiaA10SerialUnit : uint32_t {
    kSci,
    kScif,
    kCount,
};

class CasioCassiopeiaA10SerialConnector
    : public CasioCassiopeiaA10MaskedChipRegister<kCasioCassiopeiaA10SerialLines,
                                                  kCasioCassiopeiaA10LineOutputs | kCasioCassiopeiaA10LineCarrier> {
public:
    explicit CasioCassiopeiaA10SerialConnector(CerfEmulator& emu);

    void OnReady() override;
    void OnShutdown() override;

    uint16_t ReadHalf(uint32_t addr) override;

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;
    void PostRestore() override;

    MachineObject* AttachLine(CasioCassiopeiaA10SerialUnit unit, SerialLine& line);
    void Route(std::optional<CasioCassiopeiaA10SerialUnit> unit, std::optional<CasioCassiopeiaA10SerialUnit> joined);

    bool Cts() const;
    bool Carrier() const;
    void AddCtsListener(CallbackList<>::Callback listener);
    void AddCarrierListener(CallbackList<bool>::Callback listener);

protected:
    void Apply(uint16_t value) override;

private:
    static std::optional<uint32_t> LineIndex(std::optional<CasioCassiopeiaA10SerialUnit> unit);

    void SetInputs(bool cts, bool carrier);

    SerialLineSwitch   switch_;
    std::atomic<bool>  cts_{false};
    std::atomic<bool>  carrier_{false};
    CallbackList<>     cts_listeners_;
    CallbackList<bool> carrier_listeners_;
};

}
