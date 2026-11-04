#include "vga_register_file.h"

#include <cerfcore/state/state_stream.h>

namespace cerfcore {

namespace {

constexpr uint32_t kAttrIndexData = 0x3C0;
constexpr uint32_t kAttrReadData  = 0x3C1;
constexpr uint32_t kMiscWrite     = 0x3C2;
constexpr uint32_t kSeqIndex      = 0x3C4;
constexpr uint32_t kSeqData       = 0x3C5;
constexpr uint32_t kPelMask       = 0x3C6;
constexpr uint32_t kDacReadIndex  = 0x3C7;
constexpr uint32_t kDacWriteIndex = 0x3C8;
constexpr uint32_t kDacData       = 0x3C9;
constexpr uint32_t kMiscRead      = 0x3CC;
constexpr uint32_t kGcIndex       = 0x3CE;
constexpr uint32_t kGcData        = 0x3CF;
constexpr uint32_t kCrtcIndex     = 0x3D4;
constexpr uint32_t kCrtcData      = 0x3D5;

constexpr uint8_t kCrHDisp          = 0x01;
constexpr uint8_t kCrOverflow       = 0x07;
constexpr uint8_t kCrVSyncEnd       = 0x11;
constexpr uint8_t kCrVDispEnd       = 0x12;
constexpr uint8_t kCrModeControl    = 0x17;
constexpr uint8_t kCr17ScanLineDiv2 = 0x04;
constexpr uint8_t kCr11LockCr0Cr7   = 0x80;
constexpr uint8_t kCr07LineCompare8 = 0x10;

}

bool VgaRegisterFile::ReadPort(uint32_t port, uint8_t& value) {
    switch (port) {
        case kAttrIndexData: value = attr_index_; return true;
        case kAttrReadData:  value = ar_[attr_index_ & 0x1Fu]; return true;
        case kMiscRead:      value = misc_; return true;
        case kSeqIndex:      value = seq_index_; return true;
        case kSeqData:       value = sr_[seq_index_]; return true;
        case kPelMask:       value = pel_mask_; return true;
        case kDacWriteIndex: value = dac_write_idx_; return true;
        case kDacData:       value = ReadDacData(); return true;
        case kGcIndex:       value = gc_index_; return true;
        case kGcData:        value = gr_[gc_index_]; return true;
        case kCrtcIndex:     value = crtc_index_; return true;
        case kCrtcData:      value = cr_[crtc_index_]; return true;
        default:             return false;
    }
}

bool VgaRegisterFile::WritePort(uint32_t port, uint8_t value) {
    switch (port) {
        case kAttrIndexData:
            if (!attr_flipflop_) attr_index_ = value & 0x3Fu;
            else                 ar_[attr_index_ & 0x1Fu] = value;
            attr_flipflop_ = !attr_flipflop_;
            return true;
        case kMiscWrite:     misc_ = value; return true;
        case kSeqIndex:      seq_index_ = value; return true;
        case kSeqData:       sr_[seq_index_] = value; return true;
        case kPelMask:       pel_mask_ = value; return true;
        case kDacReadIndex:  dac_read_idx_  = value; dac_comp_ = 0; return true;
        case kDacWriteIndex: dac_write_idx_ = value; dac_comp_ = 0; return true;
        case kDacData:       WriteDacData(value); return true;
        case kGcIndex:       gc_index_ = value; return true;
        case kGcData:        gr_[gc_index_] = value; return true;
        case kCrtcIndex:     crtc_index_ = value; return true;
        case kCrtcData:      WriteCrtcData(value); return true;
        default:             return false;
    }
}

uint32_t VgaRegisterFile::DisplayWidth() const {
    return (static_cast<uint32_t>(cr_[kCrHDisp]) + 1u) * 8u;
}

uint32_t VgaRegisterFile::DisplayHeight() const {
    const uint32_t ov    = cr_[kCrOverflow];
    const uint32_t lines = (cr_[kCrVDispEnd] | ((ov & 0x02u) << 7) | ((ov & 0x40u) << 3)) + 1u;
    return (cr_[kCrModeControl] & kCr17ScanLineDiv2) ? lines << 1 : lines;
}

void VgaRegisterFile::WriteCrtcData(uint8_t value) {
    if ((cr_[kCrVSyncEnd] & kCr11LockCr0Cr7) && crtc_index_ <= kCrOverflow) {
        if (crtc_index_ == kCrOverflow) {
            cr_[kCrOverflow] = static_cast<uint8_t>((cr_[kCrOverflow] & ~kCr07LineCompare8) |
                                                    (value & kCr07LineCompare8));
        }
        return;
    }
    cr_[crtc_index_] = value;
}

void VgaRegisterFile::WriteDacData(uint8_t value) {
    if (dac_comp_ < 2) {
        dac_rgb_latch_[dac_comp_] = value & 0x3Fu;
    } else {
        dac_pal_[dac_write_idx_][0] = dac_rgb_latch_[0];
        dac_pal_[dac_write_idx_][1] = dac_rgb_latch_[1];
        dac_pal_[dac_write_idx_][2] = value & 0x3Fu;
    }
    if (++dac_comp_ == 3) { dac_comp_ = 0; ++dac_write_idx_; }
}

uint8_t VgaRegisterFile::ReadDacData() {
    const uint8_t v = static_cast<uint8_t>(dac_pal_[dac_read_idx_][dac_comp_] & 0x3Fu);
    if (++dac_comp_ == 3) { dac_comp_ = 0; ++dac_read_idx_; }
    return v;
}

void VgaRegisterFile::SaveState(StateWriter& w) const {
    w.WriteBytes("cr", cr_, sizeof(cr_));
    w.WriteBytes("sr", sr_, sizeof(sr_));
    w.WriteBytes("gr", gr_, sizeof(gr_));
    w.WriteBytes("ar", ar_, sizeof(ar_));
    w.Write("misc", misc_);
    w.Write("crtc_index", crtc_index_);
    w.Write("seq_index", seq_index_);
    w.Write("gc_index", gc_index_);
    w.Write("attr_index", attr_index_);
    w.Write<uint8_t>("attr_flipflop", attr_flipflop_ ? 1u : 0u);
    w.Write("pel_mask", pel_mask_);
    w.WriteBytes("dac_pal", &dac_pal_[0][0], sizeof(dac_pal_));
    w.Write("dac_write_idx", dac_write_idx_);
    w.Write("dac_read_idx", dac_read_idx_);
    w.Write("dac_comp", dac_comp_);
    w.Write("dac_rgb_latch", dac_rgb_latch_);
}

void VgaRegisterFile::RestoreState(StateReader& r) {
    r.ReadBytes("cr", cr_, sizeof(cr_));
    r.ReadBytes("sr", sr_, sizeof(sr_));
    r.ReadBytes("gr", gr_, sizeof(gr_));
    r.ReadBytes("ar", ar_, sizeof(ar_));
    r.Read("misc", misc_);
    r.Read("crtc_index", crtc_index_);
    r.Read("seq_index", seq_index_);
    r.Read("gc_index", gc_index_);
    r.Read("attr_index", attr_index_);
    uint8_t ff = 0;
    r.Read("attr_flipflop", ff);
    attr_flipflop_ = (ff != 0);
    r.Read("pel_mask", pel_mask_);
    r.ReadBytes("dac_pal", &dac_pal_[0][0], sizeof(dac_pal_));
    r.Read("dac_write_idx", dac_write_idx_);
    r.Read("dac_read_idx", dac_read_idx_);
    r.Read("dac_comp", dac_comp_);
    r.Read("dac_rgb_latch", dac_rgb_latch_);
}

}
