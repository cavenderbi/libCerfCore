#pragma once

#include <cerfcore/peripherals/peripheral_base.h>

#include <cstdint>
#include <mutex>

namespace cerfcore {

class Vr41xxPiuConverter : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override;
    void OnReady() override;

    uint32_t MmioBase() const override;
    uint32_t MmioSize() const override;

    uint16_t ReadHalf(uint32_t addr) override;
    void     WriteHalf(uint32_t addr, uint16_t value) override;

    uint32_t ReadWord  (uint32_t addr) override { return ReadWordAsHalves(addr); }
    uint64_t ReadDword (uint32_t addr) override { return ReadDwordAsHalves(addr); }
    void     WriteWord (uint32_t addr, uint32_t v) override { WriteWordAsHalves(addr, v); }
    void     WriteDword(uint32_t addr, uint64_t v) override { WriteDwordAsHalves(addr, v); }

    int  ConvertCoordinates(uint16_t pos_x, uint16_t pos_y, bool contact);
    bool ConvertCommand(uint16_t cmd, uint16_t pos_x, uint16_t pos_y, bool contact);
    bool ScanAdPorts(uint16_t ascn, uint16_t amsk);

    void InvalidatePage(int page);
    void InvalidateAdBuffer();
    void Reset();

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;

private:
    std::mutex mtx_;
    uint16_t   page_buf_[2][5] = {};
    uint16_t   next_page_      = 0;
    uint16_t   adbuf_[4]       = {};
    bool       touchscreen_    = false;
};

}
