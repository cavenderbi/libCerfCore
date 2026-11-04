#pragma once

#include <cstdint>

namespace cerfcore {

class StateWriter;
class StateReader;

class VgaRegisterFile {
public:
    bool ReadPort(uint32_t port, uint8_t& value);
    bool WritePort(uint32_t port, uint8_t value);

    void ResetAttributeFlipFlop() { attr_flipflop_ = false; }
    bool AttributeDataNext() const { return attr_flipflop_; }
    uint8_t AttributeIndex() const { return static_cast<uint8_t>(attr_index_ & 0x1Fu); }
    uint8_t Attribute(uint8_t index) const { return ar_[index & 0x1Fu]; }

    uint8_t Crtc(uint8_t index) const      { return cr_[index]; }
    uint8_t Sequencer(uint8_t index) const { return sr_[index]; }
    uint8_t Graphics(uint8_t index) const  { return gr_[index]; }
    uint8_t Misc() const                   { return misc_; }
    uint8_t SequencerIndex() const         { return seq_index_; }
    uint8_t GraphicsIndex() const          { return gc_index_; }
    uint8_t CrtcIndex() const              { return crtc_index_; }
    const uint8_t* PaletteEntry(uint8_t index) const {
        return dac_pal_[static_cast<uint8_t>(index & pel_mask_)];
    }

    uint32_t DisplayWidth() const;
    uint32_t DisplayHeight() const;

    void SaveState(StateWriter& w) const;
    void RestoreState(StateReader& r);

private:
    void WriteCrtcData(uint8_t value);
    void WriteDacData(uint8_t value);
    uint8_t ReadDacData();

    uint8_t cr_[256] = {};
    uint8_t sr_[256] = {};
    uint8_t gr_[256] = {};
    uint8_t ar_[32]  = {};
    uint8_t misc_    = 0;
    uint8_t pel_mask_ = 0;

    uint8_t crtc_index_ = 0;
    uint8_t seq_index_  = 0;
    uint8_t gc_index_   = 0;
    uint8_t attr_index_ = 0;
    bool    attr_flipflop_ = false;

    uint8_t dac_pal_[256][3]  = {};
    uint8_t dac_write_idx_    = 0;
    uint8_t dac_read_idx_     = 0;
    uint8_t dac_comp_         = 0;
    uint8_t dac_rgb_latch_[2] = {};
};

}
