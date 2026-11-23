#include "../pci/pci_host_bridge.h"
#include "../pcic82365/exca_socket_sequencer.h"
#include "../pcmcia/pcmcia_auto_insert.h"
#include "pci1225_config_space.h"
#include "pci1225_legacy_port.h"
#include "pci1225_socket_function.h"
#include "ti_cardbus_board.h"

#include "../../core/board_parts.h"
#include "ti_pci1225_part_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/host/machine_object_tree.h>
#include <cerfcore/socs/guest_cpu_reset.h>

#include <mutex>

namespace cerfcore {

namespace {

constexpr char kSocketAId[]    = "pc_card_slot_1";
constexpr char kSocketALabel[] = "PCMCIA #1";
constexpr char kSocketBId[]    = "pc_card_slot_2";
constexpr char kSocketBLabel[] = "PCMCIA #2";

class TiCardbusBridge : public Service, public Pci1225LegacyPortOwner {
public:
    explicit TiCardbusBridge(CerfEmulator& emu)
        : Service(emu),
          legacy_(shared_, mtx_, *this),
          socket_a_(emu, 0u, {kSocketAId, kSocketALabel}, shared_, mtx_, sequencer_),
          socket_b_(emu, 1u, {kSocketBId, kSocketBLabel}, shared_, mtx_, sequencer_) {}

    bool ShouldRegister() override {
        return emu_.Get<BoardParts>().Has(PartId::TiPci1225);
    }

    void OnReady() override {
        board_ = &emu_.Get<TiCardbusBoard>();
        legacy_.Attach(socket_a_, socket_b_);
        socket_a_.Attach(*board_, socket_b_, &legacy_);
        socket_b_.Attach(*board_, socket_a_, nullptr);
        ResetRegisters();
        auto& pci = emu_.Get<PciHostBridge>();
        pci.RegisterPciDevice(&socket_a_);
        pci.RegisterPciDevice(&socket_b_);
        pci.RegisterBusResetListener([this] {
            ResetFromLine();
            RedetectCards();
            UpdateLines();
        });
        emu_.Get<MachineObjectTree>().Add(&socket_a_.Slot());
        sequencer_.Run([this] {
            return ExcaSocketUpdates{{ {&socket_a_.Slot(), ExcaWriteEffect::SocketOff()},
                                       {&socket_b_.Slot(), ExcaWriteEffect::SocketOff()} }};
        });
        auto& reset = emu_.Get<GuestCpuReset>();
        reset.RegisterResetListener([this](ResetLineKind) { ResetFromLine(); });
        reset.RegisterResetReleaseListener([this] {
            RedetectCards();
            UpdateLines();
        });
        emu_.Get<PcmciaAutoInsert>().InsertDefaultNetworkCard(socket_a_.Slot());
    }

    void OnShutdown() override {
        socket_a_.Slot().OnShutdown();
        socket_b_.Slot().OnShutdown();
    }

    [[noreturn]] void RefuseLegacyAccess(const char* access, uint32_t pci_io, unsigned size,
                                         const char* reason) override {
        emu_.Get<Fatal>().Die("[PCI1225] legacy port %s of %u byte(s) at 0x%X %s", access, size, pci_io, reason);
    }

private:
    void ResetFromLine() {
        sequencer_.Run([this] {
            ResetRegisters();
            return ExcaSocketUpdates{{ {&socket_a_.Slot(), ExcaWriteEffect::SocketOff()},
                                       {&socket_b_.Slot(), ExcaWriteEffect::SocketOff()} }};
        });
    }

    void RedetectCards() {
        if (socket_a_.Slot().HasCard()) socket_a_.OnCardDetectChanged(socket_a_.Slot());
        if (socket_b_.Slot().HasCard()) socket_b_.OnCardDetectChanged(socket_b_.Slot());
    }

    void UpdateLines() {
        std::lock_guard<std::mutex> lk(mtx_);
        socket_a_.UpdateLineLocked(true);
        socket_b_.UpdateLineLocked(true);
    }

    void ResetRegisters() {
        std::lock_guard<std::mutex> lk(mtx_);
        shared_.Reset(board_->MultifunctionRoutingAtPowerOn(), board_->SerialEepromPresent(),
                      board_->GeneralPurposeInputLevels());
        socket_a_.ResetLocked();
        socket_b_.ResetLocked();
    }

    TiCardbusBoard*       board_ = nullptr;
    std::mutex            mtx_;
    ExcaSocketSequencer   sequencer_;
    Pci1225SharedConfig   shared_;
    Pci1225LegacyPort     legacy_;
    Pci1225SocketFunction socket_a_;
    Pci1225SocketFunction socket_b_;
};

REGISTER_SERVICE(TiCardbusBridge);

}

}
