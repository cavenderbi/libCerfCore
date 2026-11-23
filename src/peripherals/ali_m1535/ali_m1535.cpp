#include <cerfcore/core/service.h>

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include "../../core/board_parts.h"
#include "ali_m1535_part_id.h"
#include "../intel_i8042/i8042_controller.h"
#include "../pci/pci_device.h"
#include "../pci/pci_host_bridge.h"
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "ali_m1535_board.h"
#include "ali_m1535_bridge_config.h"
#include "ali_m1535_internal_function.h"
#include "ali_m1535_super_io.h"
#include "ali_m5229_ide.h"
#include "ali_m5237_usb.h"

#include <cstdint>
#include <initializer_list>
#include <mutex>

namespace cerfcore {

namespace {

constexpr uint32_t kExternalIsaIoFirst = AliM1535SuperIo::kInternalIoLimit;

enum : uint32_t {
    kKbcData = 0x060, kKbcStatus = 0x064,
    kPic1Cmd = 0x020, kPic1Data = 0x021,
    kPic2Cmd = 0x0A0, kPic2Data = 0x0A1,
    kElcr1   = 0x4D0, kElcr2    = 0x4D1,
};

constexpr int      kPic1CascadeLine = 2;

constexpr uint32_t kIdeIdselLine   = 15u;
constexpr uint32_t kAudioIdselLine = 17u;
constexpr uint32_t kModemIdselLine = 19u;
constexpr uint32_t kUsbIdselLine   = 21u;

constexpr AliM1535FunctionIdentity kAudioIdentity = {
    "audio", kAudioIdselLine,
    PciHeaderIdentity{{0x545110B9u, 0x04010001u, 0x0290u, 0x0000u, 0x0007u, 0x00u, 0xFFu, 0x00u}, 0x00u, 0xFFu, 0x18020100u},
    PciPowerManagement{0xDCu, 0xE6220001u},
    {PciBar{~(256u - 1u), PciBar::kIoSpace, 0u}, PciBar{~(4096u - 1u), 0u, 0u}}};
constexpr AliM1535FunctionIdentity kModemIdentity = {
    "modem", kModemIdselLine,
    PciHeaderIdentity{{0x545710B9u, 0x07030000u, 0x0290u, 0x0000u, 0x0007u, 0x00u, 0xFFu, 0x00u}, 0x00u, 0xFFu, 0x00000100u},
    PciPowerManagement{}, {PciBar{~(4096u - 1u), 0u, 0u}, PciBar{~(256u - 1u), PciBar::kIoSpace, 0u}}};
constexpr AliM1535FunctionIdentity kUsbIdentity = {
    "USB", kUsbIdselLine,
    PciHeaderIdentity{{0x523710B9u, 0x0C031003u, 0x0290u, 0x0000u, 0x0157u, 0x08u, 0xFFu, 0x00u}, 0x00u, 0xFFu, 0x50000100u},
    PciPowerManagement{0x60u, 0xC0020001u},
    {PciBar{~(4096u - 1u), 0u, 0u}, PciBar{}}};

constexpr uint8_t kSlaveFirstIrq  = 8u;
constexpr uint8_t kVectorBaseMask = 0xF8u;
constexpr int     kDefaultIrqLine = 7;
constexpr uint8_t kMasterIcw3     = 0x04u;
constexpr uint8_t kSlaveIcw3     = 0x02u;
constexpr uint8_t kIcw4Mode8086  = 0x01u;

bool IrqSelectConnected(uint8_t code) { return code != 0u && code != 2u && code != 8u && code != 13u; }

constexpr uint8_t kPirtNone       = 0xFFu;
constexpr uint8_t kPirtSelectMask = 0x0Fu;
constexpr uint8_t kPirtIrq[16]    = {kPirtNone, 9u, 3u, 10u, 4u, 5u, 7u, 6u,
                                     1u,        11u, kPirtNone, 12u, kPirtNone, 14u, kPirtNone, 15u};

struct Pic8259 {
    uint8_t imr = 0;
    uint8_t irr = 0;
    uint8_t isr = 0;
    uint8_t icw1 = 0, icw2 = 0, icw3 = 0, icw4 = 0;
    uint8_t init_step = 0;
    bool    icw4_needed = false;
    bool    read_isr = false;
    bool    poll_pending = false;

    template <typename F>
    static constexpr void Visit(Pic8259& p, F& field) {
        field("imr", p.imr);
        field("irr", p.irr);
        field("isr", p.isr);
        field("icw1", p.icw1);
        field("icw2", p.icw2);
        field("icw3", p.icw3);
        field("icw4", p.icw4);
        field("init_step", p.init_step);
        field("icw4_needed", p.icw4_needed);
        field("read_isr", p.read_isr);
        field("poll_pending", p.poll_pending);
    }

    static const char* UnmodeledCommand(uint8_t v) {
        if (v & 0x10) {
            if (v & 0x08) return "ICW1 level-triggered mode";
            if (v & 0x02) return "ICW1 single mode";
            if (!(v & 0x01)) return "ICW1 without ICW4";
            return nullptr;
        }
        if (v & 0x08) return (v & 0xC0) ? "OCW3 special mask mode" : nullptr;
        const uint8_t cmd = static_cast<uint8_t>(v >> 5);
        return (cmd == 1u || cmd == 3u) ? nullptr : "OCW2 command other than EOI";
    }

    const char* UnmodeledData(uint8_t v, bool is_master) const {
        if (init_step == 2 && v != (is_master ? kMasterIcw3 : kSlaveIcw3)) return "ICW3 cascade wiring";
        if (init_step == 3 && v != kIcw4Mode8086) return "ICW4 mode";
        return nullptr;
    }

    void WriteCmd(uint8_t v) {
        if (v & 0x10) {
            icw1 = v; icw4_needed = v & 0x01;
            imr = 0; isr = 0; read_isr = false; poll_pending = false; init_step = 1;
        } else if (v & 0x08) {
            if (v & 0x04) poll_pending = true;
            else if (v & 0x02) read_isr = (v & 0x01);
        } else {
            if (v & 0x20) {
                if (v & 0x40) {
                    isr &= static_cast<uint8_t>(~(1u << (v & 0x07)));
                } else if (isr) {
                    for (uint8_t b = 0; b < 8; ++b) {
                        if (isr & (1u << b)) { isr &= static_cast<uint8_t>(~(1u << b)); break; }
                    }
                }
            }
        }
    }

    void WriteData(uint8_t v) {
        switch (init_step) {
            case 1: icw2 = v; init_step = 2; break;
            case 2: icw3 = v; init_step = icw4_needed ? 3 : 0; break;
            case 3: icw4 = v; init_step = 0; break;
            default: imr = v; break;
        }
    }

    void RaiseEdge(int line)   { irr |= static_cast<uint8_t>(1u << line); }
    void SetCascade(bool level) {
        if (level) irr |= static_cast<uint8_t>(1u << kPic1CascadeLine);
        else       irr &= static_cast<uint8_t>(~(1u << kPic1CascadeLine));
    }
    uint8_t HigherThanInService() const {
        for (uint8_t b = 0; b < 8; ++b)
            if (isr & (1u << b)) return static_cast<uint8_t>((1u << b) - 1u);
        return 0xFFu;
    }
    uint8_t PendingUnmasked(uint8_t level) const {
        return static_cast<uint8_t>((irr | level) & ~imr & HigherThanInService());
    }
    bool    Output(uint8_t level) const { return PendingUnmasked(level) != 0; }

    int AcknowledgeLine(bool is_master, uint8_t level) {
        const uint8_t p = PendingUnmasked(level);
        if (!p) return -1;
        int b = 0;
        for (; b < 8; ++b) if (p & (1u << b)) break;
        isr |= static_cast<uint8_t>(1u << b);
        if (!(is_master && b == kPic1CascadeLine))
            irr &= static_cast<uint8_t>(~(1u << b));
        return b;
    }

    uint8_t PollAck(bool is_master, uint8_t level) {
        const int b = AcknowledgeLine(is_master, level);
        return b < 0 ? 0u : static_cast<uint8_t>(0x80 | b);
    }

    uint8_t Vector(int line) const { return static_cast<uint8_t>((icw2 & kVectorBaseMask) | line); }

    uint8_t ReadCmd(bool is_master, uint8_t level) {
        if (poll_pending) { poll_pending = false; return PollAck(is_master, level); }
        return read_isr ? isr : static_cast<uint8_t>(irr | level);
    }
    uint8_t ReadData() const { return imr; }
};

class AliM1535 : public Service, public PciDevice {
public:
    explicit AliM1535(CerfEmulator& emu)
        : Service(emu),
          super_io_(emu),
          bridge_(emu),
          ide_(emu, bridge_, kIdeIdselLine),
          audio_(emu, bridge_, AliM1535Function::kAudio, kAudioIdentity, nullptr),
          modem_(emu, bridge_, AliM1535Function::kModem, kModemIdentity, nullptr),
          usb_(emu, bridge_, AliM1535Function::kUsb, kUsbIdentity, &usb_engine_),
          usb_engine_(emu, usb_, [this](bool asserted) { SetUsbInterrupt(asserted); }) {}

    bool ShouldRegister() override {
        return emu_.Get<BoardParts>().Has(PartId::AliM1535);
    }
    void OnReady() override {
        board_    = &emu_.Get<AliM1535Board>();
        auto& pci = emu_.Get<PciHostBridge>();
        dev_      = pci.DeviceOfIdselLine(board_->BridgeIdselLine());
        ide_.SetPciDevice(pci.DeviceOfIdselLine(ide_.IdselLine()));
        for (AliM1535InternalFunction* f : {&audio_, &modem_, &usb_})
            f->SetPciDevice(pci.DeviceOfIdselLine(f->IdselLine()));
        pci.RegisterSubtractiveDecoder(this);
        pci.RegisterPciDevice(&ide_);
        pci.RegisterPciDevice(&audio_);
        pci.RegisterPciDevice(&modem_);
        pci.RegisterPciDevice(&usb_);
        auto& kbc = emu_.Get<I8042Controller>();
        kbc.SetKbdIrqSink([this] { RaiseKbcIrq(false); });
        kbc.SetAuxIrqSink([this] { RaiseKbcIrq(true); });
        usb_engine_.Attach();
        emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) {
            usb_engine_.Reset();
            std::lock_guard<std::mutex> lk(pic_mtx_);
            pic_[0] = Pic8259{};
            pic_[1] = Pic8259{};
            elcr_[0] = elcr_[1] = 0;
            super_io_.Reset();
            bridge_.Reset();
            ide_.Reset();
            audio_.Reset();
            modem_.Reset();
            usb_.Reset();
            RecomputeOutputLocked();
        });
    }

    uint8_t PciDev() const override { return dev_; }
    uint8_t PciFnc() const override { return 0u; }

    uint32_t ConfigRead(uint32_t reg) override { return bridge_.ConfigRead(reg); }
    void     ConfigWrite(uint32_t reg, uint32_t value) override {
        std::lock_guard<std::mutex> lk(pic_mtx_);
        bridge_.ConfigWrite(reg, value);
        RecomputeOutputLocked();
    }

    bool     MemClaims(uint32_t) const override { return true; }
    uint32_t MemRead(uint32_t pci_addr, unsigned size) override { Reject("ISA memory read", pci_addr, 0u, size); }
    void     MemWrite(uint32_t pci_addr, uint32_t value, unsigned size) override {
        Reject("ISA memory write", pci_addr, value, size);
    }

    bool     IoClaims(uint32_t) const override { return true; }
    uint32_t IoRead(uint32_t pci_io, unsigned size) override;
    void     IoWrite(uint32_t pci_io, uint32_t value, unsigned size) override;

    bool     IackClaims(uint32_t) const override { return true; }
    uint32_t IackRead(unsigned size) override;

    void SaveState(StateWriter& w) override;
    void RestoreState(StateReader& r) override;
    void PostRestore() override {
        usb_engine_.PostRestore();
        std::lock_guard<std::mutex> lk(pic_mtx_);
        RecomputeOutputLocked();
    }

private:
    [[noreturn]] void Reject(const char* what, uint32_t addr, uint32_t value, unsigned size) {
        emu_.Get<Fatal>().Die("ALi M1535+ legacy I/O: %s addr=0x%03X val=0x%08X size=%u", what, addr, value, size);
    }

    void RaiseKbcIrq(bool aux) {
        std::lock_guard<std::mutex> lk(pic_mtx_);
        const uint8_t irq = aux ? super_io_.MouseIrq() : super_io_.KeyboardIrq();
        if (!IrqSelectConnected(irq)) return;
        if (irq < kSlaveFirstIrq) pic_[0].RaiseEdge(irq);
        else                      pic_[1].RaiseEdge(irq - kSlaveFirstIrq);
        RecomputeOutputLocked();
    }

    void RejectLevelKbcIrqLocked(uint32_t port, uint8_t value) {
        for (uint8_t irq : {super_io_.KeyboardIrq(), super_io_.MouseIrq()}) {
            if (IrqSelectConnected(irq) && (elcr_[irq / kSlaveFirstIrq] & (1u << (irq % kSlaveFirstIrq))) != 0u)
                Reject("level-triggered keyboard controller IRQ", port, value, 1u);
        }
    }

    void WritePicLocked(int pic, bool command, uint8_t value) {
        const uint32_t port = pic == 0 ? (command ? kPic1Cmd : kPic1Data) : (command ? kPic2Cmd : kPic2Data);
        const char*    why  = command ? Pic8259::UnmodeledCommand(value) : pic_[pic].UnmodeledData(value, pic == 0);
        if (why) Reject(why, port, value, 1u);
        if (command) pic_[pic].WriteCmd(value);
        else         pic_[pic].WriteData(value);
        RecomputeOutputLocked();
    }

    uint8_t UsbIrqLocked() const { return kPirtIrq[bridge_.UsbInterruptRouting() & kPirtSelectMask]; }

    uint8_t LevelRequestsLocked(int pic) const {
        const uint8_t irq = UsbIrqLocked();
        if (!usb_level_ || irq == kPirtNone || irq / kSlaveFirstIrq != static_cast<uint8_t>(pic)) return 0u;
        const uint8_t bit = static_cast<uint8_t>(1u << (irq % kSlaveFirstIrq));
        return (elcr_[pic] & bit) != 0u ? bit : 0u;
    }

    void SetUsbInterrupt(bool asserted) {
        std::lock_guard<std::mutex> lk(pic_mtx_);
        const bool    rising = asserted && !usb_level_;
        const uint8_t irq    = UsbIrqLocked();
        usb_level_           = asserted;
        if (rising && irq != kPirtNone &&
            (elcr_[irq / kSlaveFirstIrq] & (1u << (irq % kSlaveFirstIrq))) == 0u) {
            pic_[irq / kSlaveFirstIrq].RaiseEdge(irq % kSlaveFirstIrq);
        }
        RecomputeOutputLocked();
    }

    void RecomputeOutputLocked() {
        pic_[0].SetCascade(pic_[1].Output(LevelRequestsLocked(1)));
        board_->SetInterruptLevel(pic_[0].Output(LevelRequestsLocked(0)));
    }

    AliM1535Board*           board_ = nullptr;
    uint8_t                  dev_   = 0;
    std::mutex               pic_mtx_;
    Pic8259                  pic_[2];
    uint8_t                  elcr_[2] = {0, 0};
    AliM1535SuperIo          super_io_;
    AliM1535BridgeConfig     bridge_;
    AliM5229Ide              ide_;
    AliM1535InternalFunction audio_;
    AliM1535InternalFunction modem_;
    AliM1535InternalFunction usb_;
    AliM5237Usb              usb_engine_;
    bool                     usb_level_ = false;
};

uint32_t AliM1535::IoRead(uint32_t pci_io, unsigned size) {
    if (pci_io >= kExternalIsaIoFirst) return board_->ExternalIsaIoRead(pci_io, size);
    if (size != 1u) Reject("read width", pci_io, 0u, size);
    if (pci_io == kKbcData)   return emu_.Get<I8042Controller>().ReadData();
    if (pci_io == kKbcStatus) return emu_.Get<I8042Controller>().ReadStatus();
    std::lock_guard<std::mutex> lk(pic_mtx_);
    if (AliM1535SuperIo::Decodes(pci_io)) return super_io_.Read(pci_io);
    switch (pci_io) {
        case kPic1Cmd:  { const uint8_t v = pic_[0].ReadCmd(true, LevelRequestsLocked(0));  RecomputeOutputLocked(); return v; }
        case kPic1Data: return pic_[0].ReadData();
        case kPic2Cmd:  { const uint8_t v = pic_[1].ReadCmd(false, LevelRequestsLocked(1)); RecomputeOutputLocked(); return v; }
        case kPic2Data: return pic_[1].ReadData();
        case kElcr1:    return elcr_[0];
        case kElcr2:    return elcr_[1];
        default:        Reject("read", pci_io, 0u, size);
    }
}

uint32_t AliM1535::IackRead(unsigned size) {
    if (size != 1u) Reject("interrupt acknowledge width", 0u, 0u, size);
    std::lock_guard<std::mutex> lk(pic_mtx_);
    const int master = pic_[0].AcknowledgeLine(true, LevelRequestsLocked(0));
    uint8_t   vector = pic_[0].Vector(master < 0 ? kDefaultIrqLine : master);
    if (master == kPic1CascadeLine) vector = pic_[1].Vector(pic_[1].AcknowledgeLine(false, LevelRequestsLocked(1)));
    RecomputeOutputLocked();
    return vector;
}

void AliM1535::IoWrite(uint32_t pci_io, uint32_t value, unsigned size) {
    if (pci_io >= kExternalIsaIoFirst) {
        board_->ExternalIsaIoWrite(pci_io, value, size);
        return;
    }
    if (size != 1u) Reject("write width", pci_io, value, size);
    const uint8_t v = static_cast<uint8_t>(value);
    if (pci_io == kKbcData)   { emu_.Get<I8042Controller>().WriteData(v);    return; }
    if (pci_io == kKbcStatus) { emu_.Get<I8042Controller>().WriteCommand(v); return; }
    std::lock_guard<std::mutex> lk(pic_mtx_);
    if (AliM1535SuperIo::Decodes(pci_io)) {
        super_io_.Write(pci_io, v);
        RejectLevelKbcIrqLocked(pci_io, v);
        return;
    }
    switch (pci_io) {
        case kPic1Cmd:  WritePicLocked(0, true, v);  break;
        case kPic1Data: WritePicLocked(0, false, v); break;
        case kPic2Cmd:  WritePicLocked(1, true, v);  break;
        case kPic2Data: WritePicLocked(1, false, v); break;
        case kElcr1:    elcr_[0] = v; RejectLevelKbcIrqLocked(pci_io, v); RecomputeOutputLocked(); break;
        case kElcr2:    elcr_[1] = v; RejectLevelKbcIrqLocked(pci_io, v); RecomputeOutputLocked(); break;
        default:        Reject("write", pci_io, value, size);
    }
}

void AliM1535::SaveState(StateWriter& w) {
    std::lock_guard<std::mutex> lk(pic_mtx_);
    static_assert(StateVisitCoversAllBytes<Pic8259>(
                      [](Pic8259& p, StateFieldBytes& f) { Pic8259::Visit(p, f); }),
                  "Pic8259::Visit must name or skip every field of Pic8259");
    StateWriteField field(w);
    for (Pic8259& p : pic_) Pic8259::Visit(p, field);
    w.WriteBytes("elcr", elcr_, sizeof(elcr_));
    w.Write("usb_level", usb_level_);
    super_io_.SaveState(w);
    bridge_.SaveState(w);
    usb_engine_.SaveState(w);
    emu_.Get<I8042Controller>().SaveState(w);
}

void AliM1535::RestoreState(StateReader& r) {
    std::lock_guard<std::mutex> lk(pic_mtx_);
    StateReadField field(r);
    for (Pic8259& p : pic_) Pic8259::Visit(p, field);
    r.ReadBytes("elcr", elcr_, sizeof(elcr_));
    r.Read("usb_level", usb_level_);
    super_io_.RestoreState(r);
    bridge_.RestoreState(r);
    usb_engine_.RestoreState(r);
    emu_.Get<I8042Controller>().RestoreState(r);
}

}

REGISTER_SERVICE(AliM1535);

}
