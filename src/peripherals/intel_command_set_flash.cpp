#include "intel_command_set_flash.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/cpu/emulated_memory.h>
#include "peripheral_dispatcher.h"
#include <cerfcore/state/state_stream.h>

#include <cstring>

namespace cerfcore {

void IntelCommandSetFlash::OnReady() {
    (void)emu_.Get<EmulatedMemory>().Translate(MmioBase());
    block_locked_.assign(NumBlocks(), 0u);
    emu_.Get<PeripheralDispatcher>().Register(this);
    emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind kind) {
        if (ResetPinAsserted(kind)) ResetDevice();
    });
}

uint8_t* IntelCommandSetFlash::Host(uint32_t addr) {
    return emu_.Get<EmulatedMemory>().TryTranslate(addr);
}

void IntelCommandSetFlash::SaveAndWrite(uint32_t addr, uint8_t v) {
    uint8_t* p = Host(addr);
    if (!p) return;
    shadow_.push_back({addr, *p});
    *p = v;
}

void IntelCommandSetFlash::SaveAndWriteWord(uint32_t addr, uint32_t word) {
    for (uint32_t i = 0; i < 4u; ++i)
        SaveAndWrite(addr + i, uint8_t(word >> (8u * i)));
}

void IntelCommandSetFlash::EmitLane(uint32_t addr, uint32_t value) {
    const uint32_t dw = DeviceWidth();
    for (uint32_t c = 0; c < Parallel(); ++c)
        for (uint32_t j = 0; j < dw; ++j) SaveAndWrite(addr + c * dw + j, uint8_t(value >> (8u * j)));
}

void IntelCommandSetFlash::RestoreArray() {
    for (auto it = shadow_.rbegin(); it != shadow_.rend(); ++it)
        if (uint8_t* p = Host(it->first)) *p = it->second;
    shadow_.clear();
}

void IntelCommandSetFlash::ResetToReadArray() {
    RestoreArray();
    mode_          = Mode::kArray;
    buf_remaining_ = 0u;
    buf_locked_    = false;
    status_errors_ = 0u;
    buffer_.clear();
}

void IntelCommandSetFlash::PresentStatus(uint32_t addr, uint8_t sr) {
    status_errors_ = uint8_t(status_errors_ | (sr & kSrErrorBits));
    RestoreArray();
    EmitLane(addr, uint8_t((sr & ~kSrErrorBits) | status_errors_));
}

void IntelCommandSetFlash::PresentRegister(uint32_t addr, uint8_t value) {
    RestoreArray();
    EmitLane(addr, value);
}

uint32_t IntelCommandSetFlash::WriteBufferWords(uint32_t addr, uint32_t value) {
    const uint32_t count = value & 0xFFu;
    const uint32_t limit = DeviceWidth() == 1u ? 0x1Fu : 0x0Fu;
    if (count > limit) HaltUnsupportedAccess("flash write-to-buffer word count", addr, value);
    return count + 1u;
}

uint32_t IntelCommandSetFlash::BlockIndexChecked(uint32_t addr) {
    const uint32_t i = BlockIndex(addr);
    if (i >= block_locked_.size())
        HaltUnsupportedAccess("flash block index out of range", addr, 0);
    return i;
}

/* MMCRestore.exe sub_130E8 reads the manufacturer at a data-block base
   0x1FFA0000; block lock bit at block_base + kLockReadOffset (datasheet Table 5). */
void IntelCommandSetFlash::PresentIdentifier(uint32_t addr) {
    RestoreArray();
    const uint32_t bw = BusBytes();
    EmitLane(MmioBase(),      Manufacturer());
    EmitLane(MmioBase() + bw, Device());
    const uint32_t qbase = BlockBase(BlockIndex(addr));
    if (qbase != MmioBase()) {
        EmitLane(qbase,      Manufacturer());
        EmitLane(qbase + bw, Device());
    }
    const uint32_t n = NumBlocks();
    for (uint32_t i = 0; i < n; ++i)
        EmitLane(BlockBase(i) + kLockReadOffset, block_locked_[i] & 0x01u);
}

void IntelCommandSetFlash::PresentCfi() {
    const uint8_t* cfi = CfiTable();
    const uint32_t n   = CfiCount();
    if (!cfi || n == 0u) HaltUnsupportedAccess("flash CFI query", MmioBase(), 0x98);
    RestoreArray();
    const uint32_t bw  = BusBytes();
    const uint32_t win = MmioBase() + kCfiFirst * bw;
    for (uint32_t i = 0; i < n; ++i) EmitLane(win + i * bw, cfi[i]);
}

bool IntelCommandSetFlash::ProgramInto(uint32_t addr, uint32_t value, uint32_t width) {
    if (block_locked_[BlockIndexChecked(addr)]) return false;   /* datasheet p20: locked block aborts */
    if (uint8_t* h = Host(addr)) {
        ArrayWillChange(addr, width);
        for (uint32_t i = 0; i < width; ++i) h[i] &= uint8_t(value >> (8u * i));
        ArrayStored(addr, width);
    }
    return true;
}

void IntelCommandSetFlash::CommitProgram(uint32_t addr, uint32_t value, uint32_t width) {
    RestoreArray();
    if (!ProgramInto(addr, value, width))
        PresentStatus(addr, kSrReady | kSrProgErr | kSrLockAbort);
    else
        PresentStatus(addr);
}

void IntelCommandSetFlash::LockSetupCommand(uint32_t addr, uint8_t cmd) {
    if (cmd == 0x01u) {
        block_locked_[BlockIndexChecked(addr)] = 1u;
        PresentStatus(addr);
    } else if (cmd == 0xD0u) {
        block_locked_.assign(block_locked_.size(), 0u);
        PresentStatus(addr);
    } else {
        PresentStatus(addr, kSrReady | kSrEraseErr | kSrProgErr);
    }
}

void IntelCommandSetFlash::ConfirmBufferedWrite(uint32_t addr, uint32_t value) {
    RestoreArray();
    if ((value & 0xFFu) != 0xD0u) {
        PresentStatus(addr, kSrReady | kSrEraseErr | kSrProgErr);
    } else if (buf_locked_) {
        PresentStatus(addr, kSrReady | kSrProgErr | kSrLockAbort);
    } else {
        for (const BufferedWrite& w : buffer_) ProgramInto(w.addr, w.value, w.width);
        PresentStatus(addr);
    }
    buffer_.clear();
}

void IntelCommandSetFlash::EraseBlock(uint32_t addr) {
    RestoreArray();
    const uint32_t blk   = EraseBlockBytes();
    const uint32_t bbase = addr & ~(blk - 1u);
    if (uint8_t* h = Host(bbase)) {
        ArrayWillChange(bbase, blk);
        std::memset(h, 0xFF, blk);
        ArrayStored(bbase, blk);
    }
}

void IntelCommandSetFlash::Command(uint32_t addr, uint32_t value, uint32_t width) {
    switch (mode_) {
    case Mode::kProgramSetup:
        CommitProgram(addr, value, width);
        mode_ = Mode::kArray;
        return;
    case Mode::kEraseSetup:
        if ((value & 0xFFu) != 0xD0u)
            PresentStatus(addr, kSrReady | kSrEraseErr | kSrProgErr);
        else if (block_locked_[BlockIndexChecked(addr)])
            PresentStatus(addr, kSrReady | kSrEraseErr | kSrLockAbort);
        else { EraseBlock(addr); PresentStatus(addr); }
        mode_ = Mode::kArray;
        return;
    case Mode::kLockSetup:
        LockSetupCommand(addr, uint8_t(value & 0xFFu));
        mode_ = Mode::kArray;
        return;
    case Mode::kWriteBufCount:                      /* MMCRestore.exe sub_131F8 writes 0x0F = 16 words */
        buf_remaining_ = WriteBufferWords(addr, value);
        buf_block_     = BlockIndexChecked(addr);
        buf_locked_    = block_locked_[buf_block_] != 0u;
        buffer_.clear();
        PresentStatus(addr);
        mode_ = Mode::kWriteBufData;
        return;
    case Mode::kWriteBufData:
        if (BlockIndex(addr) != buf_block_) {
            buffer_.clear();
            PresentStatus(addr, kSrReady | kSrEraseErr | kSrProgErr);
            mode_ = Mode::kArray;
            return;
        }
        buffer_.push_back(BufferedWrite{addr, value, width});
        PresentStatus(addr);
        if (--buf_remaining_ == 0u) mode_ = Mode::kWriteBufConfirm;
        return;
    case Mode::kWriteBufConfirm:
        ConfirmBufferedWrite(addr, value);
        mode_ = Mode::kArray;
        return;
    case Mode::kArray:
        break;
    }

    const uint8_t cmd = DecodeCommand(value, width);
    if (!CommandEnabled(cmd)) HaltUnsupportedAccess("flash command", addr, value);
    if (PartCommand(addr, cmd)) return;
    switch (cmd) {
        case 0xFF: RestoreArray();               break;
        case 0x90: PresentIdentifier(addr);      break;
        case 0x98: PresentCfi();                 break;
        case 0x70: PresentStatus(addr);          break;
        case 0x50: ClearStatus();                break;
        case 0x40:
        case 0x10: mode_ = Mode::kProgramSetup;  break;
        case 0x20: mode_ = Mode::kEraseSetup;    break;
        case 0xE8: {
            const bool refused = WriteBufferRefused();
            if (WriteBufferUsesXsr()) PresentRegister(addr, refused ? 0x00u : kXsrBufferAvailable);
            else PresentStatus(addr);
            if (!refused) mode_ = Mode::kWriteBufCount;
            break;
        }
        case 0x60: mode_ = Mode::kLockSetup;
                   PresentStatus(addr);          break;
        case 0xB0:
        case 0xD0:                               break;
        default:
            HaltUnsupportedAccess("flash command", addr, value);
    }
}

uint8_t  IntelCommandSetFlash::ReadByte(uint32_t addr) { return emu_.Get<EmulatedMemory>().ReadByte(addr); }
uint16_t IntelCommandSetFlash::ReadHalf(uint32_t addr) { return emu_.Get<EmulatedMemory>().ReadHalf(addr); }
uint32_t IntelCommandSetFlash::ReadWord(uint32_t addr) { return emu_.Get<EmulatedMemory>().ReadWord(addr); }
void     IntelCommandSetFlash::WriteByte(uint32_t addr, uint8_t  v) { Command(addr, v, 1); }
void     IntelCommandSetFlash::WriteHalf(uint32_t addr, uint16_t v) { Command(addr, v, 2); }
void     IntelCommandSetFlash::WriteWord(uint32_t addr, uint32_t v) { Command(addr, v, 4); }

uint8_t IntelCommandSetFlash::DecodeCommand(uint32_t value, uint32_t /*width*/) {
    return uint8_t(value & 0xFFu);
}

void IntelCommandSetFlash::SaveState(StateWriter& w) {
    w.Write("mode", mode_);
    w.Write("buf_remaining", buf_remaining_);
    w.Write("buf_locked", buf_locked_);
    w.Write("buf_block", buf_block_);
    w.Write("status_errors", status_errors_);
    w.Write<uint64_t>("buffer_count", buffer_.size());
    for (const BufferedWrite& b : buffer_) {
        w.Write("buffer_address", b.addr);
        w.Write("buffer_value", b.value);
        w.Write("buffer_width", b.width);
    }
    w.Write<uint64_t>("shadow_count", shadow_.size());
    for (const auto& pr : shadow_) {
        w.Write("shadow_address", pr.first);
        w.Write("shadow_original_byte", pr.second);
        w.Write("shadow_presented_byte", *Host(pr.first));
    }
    w.WriteBytes("block_locked", block_locked_.data(), block_locked_.size());
}

void IntelCommandSetFlash::RestoreState(StateReader& r) {
    r.Read("mode", mode_);
    r.Read("buf_remaining", buf_remaining_);
    r.Read("buf_locked", buf_locked_);
    r.Read("buf_block", buf_block_);
    r.Read("status_errors", status_errors_);
    uint64_t count = 0; r.Read("buffer_count", count);
    buffer_.clear();
    for (uint64_t i = 0; i < count; ++i) {
        BufferedWrite b;
        r.Read("buffer_address", b.addr); r.Read("buffer_value", b.value); r.Read("buffer_width", b.width);
        buffer_.push_back(b);
    }
    const bool live = ArrayIsStorage();
    uint64_t n = 0; r.Read("shadow_count", n);
    std::vector<std::pair<uint32_t, uint8_t>> shadow;
    std::vector<uint8_t>                      presented;
    for (uint64_t i = 0; i < n; ++i) {
        uint32_t a = 0; uint8_t b = 0; uint8_t p = 0;
        r.Read("shadow_address", a); r.Read("shadow_original_byte", b);
        r.Read("shadow_presented_byte", p);
        shadow.push_back({a, live ? *Host(a) : b});
        presented.push_back(p);
    }
    r.ReadBytes("block_locked", block_locked_.data(), block_locked_.size());
    shadow_ = std::move(shadow);
    for (size_t i = 0; i < shadow_.size(); ++i) *Host(shadow_[i].first) = presented[i];
}

}
