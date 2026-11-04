#include "ite_it8181_gui.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/state/state_stream.h>

namespace cerfcore {

namespace {

constexpr uint32_t kBsle = 0x00u;
constexpr uint32_t kBdls = 0x04u;
constexpr uint32_t kBpoa = 0x08u;
constexpr uint32_t kBwh  = 0x0Cu;
constexpr uint32_t kBso  = 0x10u;
constexpr uint32_t kFcr  = 0x14u;
constexpr uint32_t kBcr  = 0x18u;
constexpr uint32_t kBc   = 0x1Cu;
constexpr uint32_t kBs   = 0x20u;
constexpr uint32_t kAsds = 0x24u;
constexpr uint32_t kLet  = 0x28u;
constexpr uint32_t kLst  = 0x2Cu;
constexpr uint32_t kLsb  = 0x30u;
constexpr uint32_t kLsr  = 0x34u;

constexpr uint32_t kSuspendFirst = 0x64u;

constexpr uint32_t kCursorCtrl   = 0x100u;
constexpr uint32_t kCursorColor0 = 0x110u;
constexpr uint32_t kCursorColor1 = 0x114u;
constexpr uint32_t kCursorPos    = 0x120u;
constexpr uint32_t kHostPort     = 0x4000u;

constexpr uint32_t kBusy          = 0x100u;
constexpr uint32_t kCursorEnable  = 0x40u;
constexpr uint32_t kCursorWhite   = 0xFFFFFFFFu;

}

void IteIt8181Gui::PowerOnReset() {
    regs_ = IteIt8181Blitter::Registers{};
    for (uint32_t& v : suspend_regs_) v = 0u;
    cursor_ctrl_   = 0u;
    cursor_color0_ = 0u;
    cursor_color1_ = 0u;
    cursor_pos_    = 0u;
    host_.clear();
    host_needed_ = 0u;
}

uint32_t* IteIt8181Gui::BlitRegister(uint32_t off) {
    switch (off) {
        case kBsle: return &regs_.bsle;
        case kBdls: return &regs_.bdls;
        case kBpoa: return &regs_.bpoa;
        case kBwh:  return &regs_.bwh;
        case kBso:  return &regs_.bso;
        case kFcr:  return &regs_.fcr;
        case kBcr:  return &regs_.bcr;
        case kAsds: return &regs_.asds;
        case kLet:  return &regs_.let;
        case kLst:  return &regs_.lst;
        case kLsb:  return &regs_.lsb;
        case kLsr:  return &regs_.lsr;
        default:    return nullptr;
    }
}

uint32_t IteIt8181Gui::Read(uint32_t off) {
    if (off == kBs) return host_needed_ ? kBusy : 0u;
    if (off == kCursorPos) return cursor_pos_;
    if (off >= kSuspendFirst && off < kSuspendFirst + kSuspendRegs * 4u && (off & 3u) == 0u) {
        return suspend_regs_[(off - kSuspendFirst) / 4u];
    }
    emu_.Get<Fatal>().Die("IteIt8181Gui: read of GUI register 0x%04X", off);
}

void IteIt8181Gui::Write(uint32_t off, uint32_t value) {
    if (host_needed_ && off != kHostPort) {
        emu_.Get<Fatal>().Die("IteIt8181Gui: write 0x%08X to GUI register 0x%04X while a "
                              "host-source blit waits for %u bytes", value, off,
                              host_needed_ - static_cast<uint32_t>(host_.size()));
    }
    if (uint32_t* reg = BlitRegister(off)) {
        *reg = value;
        return;
    }
    switch (off) {
        case kBc:           StartCommand(value); return;
        case kHostPort:     PushHostData(value); return;
        case kCursorPos:    cursor_pos_ = value; return;
        case kCursorColor0: WriteCursorColor(cursor_color0_, off, value); return;
        case kCursorColor1: WriteCursorColor(cursor_color1_, off, value); return;
        case kCursorCtrl:
            if (value & ~kCursorEnable) {
                emu_.Get<Fatal>().Die("IteIt8181Gui: cursor control 0x%08X sets an offset, a "
                                      "feature or an image address", value);
            }
            cursor_ctrl_ = value;
            return;
        default: break;
    }
    if (off >= kSuspendFirst && off < kSuspendFirst + kSuspendRegs * 4u && (off & 3u) == 0u) {
        suspend_regs_[(off - kSuspendFirst) / 4u] = value;
        return;
    }
    emu_.Get<Fatal>().Die("IteIt8181Gui: write 0x%08X to GUI register 0x%04X", value, off);
}

void IteIt8181Gui::WriteCursorColor(uint32_t& color, uint32_t off, uint32_t value) {
    if (value != 0u && value != kCursorWhite) {
        emu_.Get<Fatal>().Die("IteIt8181Gui: cursor colour 0x%08X at 0x%04X is not black or "
                              "white", value, off);
    }
    color = value;
}

void IteIt8181Gui::StartCommand(uint32_t bc) {
    regs_.bc = bc;
    blitter_.CheckCommand(regs_);
    const size_t need = blitter_.HostBytes(regs_);
    if (need == 0u) {
        blitter_.Execute(regs_, nullptr);
        return;
    }
    host_.clear();
    host_.reserve(need);
    host_needed_ = static_cast<uint32_t>(need);
}

void IteIt8181Gui::PushHostData(uint32_t value) {
    if (!host_needed_) {
        emu_.Get<Fatal>().Die("IteIt8181Gui: host data 0x%08X with no host-source blit", value);
    }
    for (uint32_t i = 0; i < 4u; ++i) host_.push_back(static_cast<uint8_t>(value >> (8u * i)));
    if (host_.size() < host_needed_) return;
    blitter_.Execute(regs_, host_.data());
    host_.clear();
    host_needed_ = 0u;
}

bool IteIt8181Gui::CursorShown() const {
    return (cursor_ctrl_ & kCursorEnable) != 0u;
}

void IteIt8181Gui::SaveState(StateWriter& w) const {
    w.Write("gui_bsle", regs_.bsle);
    w.Write("gui_bdls", regs_.bdls);
    w.Write("gui_bpoa", regs_.bpoa);
    w.Write("gui_bwh", regs_.bwh);
    w.Write("gui_bso", regs_.bso);
    w.Write("gui_fcr", regs_.fcr);
    w.Write("gui_bcr", regs_.bcr);
    w.Write("gui_bc", regs_.bc);
    w.Write("gui_asds", regs_.asds);
    w.Write("gui_let", regs_.let);
    w.Write("gui_lst", regs_.lst);
    w.Write("gui_lsb", regs_.lsb);
    w.Write("gui_lsr", regs_.lsr);
    w.Write("gui_suspend_regs", suspend_regs_);
    w.Write("cursor_ctrl", cursor_ctrl_);
    w.Write("cursor_color0", cursor_color0_);
    w.Write("cursor_color1", cursor_color1_);
    w.Write("cursor_pos", cursor_pos_);
    w.Write("host_needed", host_needed_);
    w.Write("host_len", static_cast<uint32_t>(host_.size()));
    w.WriteBytes("host_data", host_.data(), host_.size());
}

void IteIt8181Gui::RestoreState(StateReader& r) {
    r.Read("gui_bsle", regs_.bsle);
    r.Read("gui_bdls", regs_.bdls);
    r.Read("gui_bpoa", regs_.bpoa);
    r.Read("gui_bwh", regs_.bwh);
    r.Read("gui_bso", regs_.bso);
    r.Read("gui_fcr", regs_.fcr);
    r.Read("gui_bcr", regs_.bcr);
    r.Read("gui_bc", regs_.bc);
    r.Read("gui_asds", regs_.asds);
    r.Read("gui_let", regs_.let);
    r.Read("gui_lst", regs_.lst);
    r.Read("gui_lsb", regs_.lsb);
    r.Read("gui_lsr", regs_.lsr);
    r.Read("gui_suspend_regs", suspend_regs_);
    r.Read("cursor_ctrl", cursor_ctrl_);
    r.Read("cursor_color0", cursor_color0_);
    r.Read("cursor_color1", cursor_color1_);
    r.Read("cursor_pos", cursor_pos_);
    r.Read("host_needed", host_needed_);
    uint32_t len = 0u;
    r.Read("host_len", len);
    host_.resize(len);
    r.ReadBytes("host_data", host_.data(), host_.size());
}

}
