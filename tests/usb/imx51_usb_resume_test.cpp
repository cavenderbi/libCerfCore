#include "../../src/socs/imx51/imx51_usboh3.h"
#include "../../src/peripherals/usb/usb_hub.h"
#include "../../src/socs/imx51/imx51_usb_device_transfers.h"
#include "../../src/socs/imx51/imx51_usb_transceiver_clocks.h"
#include "../../src/socs/irq_controller.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/state/state_stream.h>
#include <cstdio>
#include <stdexcept>

using namespace cerfcore;

namespace cerfcore {
struct Imx51Usboh3TestAccess {
    static void Initialize(Imx51Usboh3& controller, GuestCycleClock& clock) {
        controller.clock_ = &clock;
        controller.frame_event_ = clock.Add([] {});
        controller.irq_event_ = clock.Add([] {});
        controller.resume_event_ = clock.Add([&] { controller.CompleteHostResume(); });
        clock.RegisterRateListener([&] { controller.OnCpuRateChanged(); });
        controller.transfers_ = &controller.emu_.Get<Imx51UsbDeviceTransfers>();
        controller.transceiver_clocks_ = &controller.emu_.Get<Imx51UsbTransceiverClocks>();
        controller.frame_index_.Attach();
        controller.ResetController();
    }
    static void Reset(Imx51Usboh3& controller) { controller.ResetController(); }
    static int64_t ResumeAt(Imx51Usboh3& controller) { return controller.resume_at_ns_; }
};
}

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class Clock : public GuestCycleClock {
public:
    using GuestCycleClock::GuestCycleClock;
    uint64_t Cycles() override { return cycles_; }
    void Advance(uint64_t cycles) { cycles_ += cycles; OnDispatch(); }
protected:
    Rate InitialRate() override { return {600000001, 1}; }
    void SetCycles(uint64_t cycles) override { cycles_ = cycles; }
    void PublishDeadline(uint64_t) override {}
private:
    uint64_t cycles_ = 0;
};

class Interrupts : public IrqController {
public:
    using IrqController::IrqController;
    void AssertIrq(int source) override { if (source == 18) pending = true; }
    void DeAssertIrq(int source) override { if (source == 18) pending = false; }
    void AssertSubIrq(int, int) override {}
    bool pending = false;
};

class Controller : public Imx51Usboh3 {
public:
    using Imx51Usboh3::Imx51Usboh3;
    void OnReady() override {}
};

class Hub : public UsbHub {
public:
    Hub() : UsbHub(2) {}
    uint32_t StateKind() const override { return 4; }
};

struct Fixture {
    CerfEmulator emu{{}, 0, nullptr};
    Clock* clock = nullptr;
    Interrupts* irq = nullptr;
    Controller controller{emu};
    Fixture() {
        auto time = std::make_unique<Clock>(emu);
        clock = time.get();
        emu.AddCandidate<GuestCycleClock>(std::move(time), false);
        auto interrupts = std::make_unique<Interrupts>(emu);
        irq = interrupts.get();
        emu.AddCandidate<IrqController>(std::move(interrupts), false);
        emu.AddCandidate<Imx51UsbDeviceTransfers>(std::make_unique<Imx51UsbDeviceTransfers>(emu), false);
        emu.AddCandidate<Imx51UsbTransceiverClocks>(std::make_unique<Imx51UsbTransceiverClocks>(emu), false);
        emu.Get<GuestCycleClock>();
        Imx51Usboh3TestAccess::Initialize(controller, *clock);
        controller.OtgHostRootPort().SetRestoreFactory([](uint32_t kind) -> std::unique_ptr<UsbDevice> {
            return kind == 4 ? std::make_unique<Hub>() : nullptr;
        });
        controller.OtgHostRootPort().Attach(std::make_unique<Hub>());
        Write(0x184, 0x100);
        Write(0x144, 0xFFFFFFFF);
        Write(0x148, 4);
    }
    void Write(uint32_t offset, uint32_t value) { controller.WriteWord(0x73F80000 + offset, value); }
    uint32_t Port() { return controller.ReadWord(0x73F80184); }
    void Wake() {
        Write(0x184, 0x80);
        controller.OnPortResumeRequested(0);
        Require((Port() & 0xC0) == 0xC0 && irq->pending, "resume raises FPR and PCD IRQ");
    }
    void CheckCancelled(const char* message) {
        Require((Port() & 0xC0) == 0 && Imx51Usboh3TestAccess::ResumeAt(controller) < 0, message);
        const auto port = Port();
        clock->Advance(60000000);
        Require(Port() == port, "cancelled event cannot modify port later");
    }
};

void CheckDuration() {
    Fixture f;
    f.Wake();
    f.Write(0x184, 0);
    Require((f.Port() & 0xC0) == 0xC0, "writing zero cannot end hardware resume");
    f.clock->Advance(12000000);
    Require((f.Port() & 0x40) != 0, "resume must last at least twenty milliseconds");
    f.clock->Advance(1);
    Require((f.Port() & 0xC0) == 0, "resume completes on rounded-up cycle");
}

void CheckCancellation() {
    { Fixture f; f.Wake(); f.Write(0x184, 0x100); f.CheckCancelled("port reset cancels resume"); }
    { Fixture f; f.Wake(); f.Write(0x184, f.Port() | 0x100); f.CheckCancelled("port reset overrides preserved resume bits"); }
    { Fixture f; f.Wake(); f.Write(0x140, 2); f.CheckCancelled("controller reset cancels resume"); }
    { Fixture f; f.Wake(); f.Write(0x1A8, 2); f.CheckCancelled("device role cancels host resume"); }
    { Fixture f; f.Wake(); Imx51Usboh3TestAccess::Reset(f.controller); f.CheckCancelled("SoC reset cancels resume"); }
    { Fixture f; f.Wake(); f.controller.OtgHostRootPort().Detach(); f.CheckCancelled("disconnect cancels resume"); }
}

void CheckRateChange() {
    Fixture f;
    f.Wake();
    f.clock->Advance(6000000);
    f.clock->SetClockHz(300000000);
    f.clock->Advance(3000000);
    Require((f.Port() & 0x40) != 0, "rate change preserves remaining resume duration");
    f.clock->Advance(1);
    Require((f.Port() & 0xC0) == 0, "rescaled resume completes");
}

void CheckPendingRestore() {
    Fixture source;
    source.Wake();
    source.clock->Advance(6000000);
    const char* path = "imx51-usb-resume-state.bin";
    {
        StateWriter writer(source.emu, path);
        source.controller.SaveState(writer);
        Require(writer.Commit(), "save pending resume");
    }
    Fixture restored;
    restored.clock->Advance(60000000);
    {
        StateReader reader(restored.emu, path);
        restored.controller.RestoreState(reader);
        Require(reader.Ok(), "restore pending resume");
    }
    restored.controller.PostRestore();
    Require((restored.Port() & 0xC0) == 0xC0 && restored.irq->pending, "resume IRQ restored");
    restored.clock->Advance(6000000);
    Require((restored.Port() & 0x40) != 0, "restore rebases remaining duration");
    restored.clock->Advance(2);
    Require((restored.Port() & 0xC0) == 0, "restored resume completes");
    std::remove(path);
}
}

int main() {
    try {
        CheckDuration();
        CheckCancellation();
        CheckRateChange();
        CheckPendingRestore();
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
