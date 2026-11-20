#pragma once

#include <cerfcore/peripherals/peripheral_base.h>

#include <array>
#include <cstdint>

namespace cerfcore {

class CasioCassiopeiaA10VideoMemory : public Peripheral {
public:
    static constexpr uint32_t kRowStride = 0x100u;

    using Peripheral::Peripheral;

    bool ShouldRegister() override;
    void OnReady() override;

    uint32_t MmioBase() const override;
    uint32_t MmioSize() const override;

    uint8_t  ReadByte(uint32_t addr) override;
    uint16_t ReadHalf(uint32_t addr) override;
    uint32_t ReadWord(uint32_t addr) override;
    void     WriteByte(uint32_t addr, uint8_t value) override;
    void     WriteHalf(uint32_t addr, uint16_t value) override;
    void     WriteWord(uint32_t addr, uint32_t value) override;

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;

    const uint8_t* Frame() const;

private:
    static constexpr uint32_t kControllerBytes = 4u;

    const uint8_t*                        frame_ = nullptr;
    std::array<uint8_t, kControllerBytes> controller_{};
};

}
