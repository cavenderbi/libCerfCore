#pragma once

#include <cerfcore/peripherals/peripheral_base.h>

#include <cstdint>

namespace cerfcore {

class PcmciaSlot;

class CasioCassiopeiaA10CardWindows : public Peripheral {
public:
    static constexpr uint32_t kSpaceBytes = 0x01000000u;

    using Peripheral::Peripheral;

    void OnReady() override;

    uint32_t MmioSize() const override;

    uint8_t  ReadByte(uint32_t addr) override;
    void     WriteByte(uint32_t addr, uint8_t value) override;
    uint16_t ReadHalf(uint32_t addr) override;
    void     WriteHalf(uint32_t addr, uint16_t value) override;

protected:
    enum class Space { kAttribute, kCommon, kIo };

    virtual PcmciaSlot& CardSlot() = 0;
    virtual bool        SpaceSelected(Space space);

private:
    Space SpaceOf(uint32_t addr, const char* access, uint32_t value);

    PcmciaSlot* slot_ = nullptr;
};

}
