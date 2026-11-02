#include "compactflash_ata_core.h"

#include "compactflash_ata_commands.h"
#include <cerfcore/state/state_stream.h>

namespace cerfcore {

namespace {

using Tf = CompactFlashTaskFile;

}

CompactFlashAtaCore::CompactFlashAtaCore(CerfEmulator& emu_ref, const std::string& image_path)
    : emu(emu_ref), media(emu_ref, image_path), power(emu_ref), xfer(emu_ref) {}

uint8_t CompactFlashAtaCore::IdleStatus() const {
    return media.IsOpen() ? static_cast<uint8_t>(Tf::kStatusReady | Tf::kStatusSeek) : uint8_t{0};
}

void CompactFlashAtaCore::PowerOn() {
    tf.PowerOn();
    tf.status    = IdleStatus();
    xfer.Stop();
    sectors_left = 0;
    intrq        = false;
    multiple     = false;
    sense        = CompactFlashSense::kNone;
    soft_reset_restores_defaults = true;
    power.PowerOn();
}

void CompactFlashAtaCore::PowerOff() {
    intrq        = false;
    tf.status    = 0;
    xfer.Stop();
    sectors_left = 0;
}

void CompactFlashAtaCore::SoftResetComplete() {
    tf.ApplySignature();
    if (soft_reset_restores_defaults) {
        multiple    = false;
        tf.geometry = Tf::kDefaultGeometry;
        power.SetDefaults();
    }
    power.Wake();
    tf.status = IdleStatus();
    sense     = CompactFlashSense::kSelfTestOk;
    power.MarkActivity();
    IrqAssert();
}

bool CompactFlashAtaCore::InterruptRequest() const {
    return intrq && (tf.dev_ctrl & Tf::kDevCtlNoIrq) == 0u;
}

void CompactFlashAtaCore::IrqAssert() {
    if ((tf.dev_ctrl & Tf::kDevCtlNoIrq) != 0u) return;
    intrq = true;
}

void CompactFlashAtaCore::Complete() {
    tf.status = Tf::kStatusReady | Tf::kStatusSeek;
    sense     = CompactFlashSense::kNone;
    power.MarkActivity();
    IrqAssert();
}

void CompactFlashAtaCore::Fail(uint8_t error_bits, uint8_t sense_code) {
    tf.error  = error_bits;
    tf.status = Tf::kStatusReady | Tf::kStatusSeek | Tf::kStatusError;
    sense     = sense_code;
    xfer.Stop();
    power.MarkActivity();
    IrqAssert();
}

bool CompactFlashAtaCore::Succeeded(CompactFlashAddressFault fault) {
    if (fault == CompactFlashAddressFault::kNone)
        return true;
    Fail(Tf::kErrorIdnf, fault == CompactFlashAddressFault::kInvalidAddress ? CompactFlashSense::kInvalidAddress
                                                                             : CompactFlashSense::kAddressTooLarge);
    return false;
}

void CompactFlashAtaCore::BeginRead(CompactFlashTransfer kind) {
    xfer.Start(kind);
    tf.status = Tf::kStatusReady | Tf::kStatusSeek | Tf::kStatusDrq;
    power.MarkActivity();
    IrqAssert();
}

void CompactFlashAtaCore::BeginWrite(CompactFlashTransfer kind, uint32_t sectors) {
    sectors_left = sectors;
    xfer.Start(kind);
    tf.status = Tf::kStatusReady | Tf::kStatusSeek | Tf::kStatusDrq;
    power.MarkActivity();
}

void CompactFlashAtaCore::FinishTransfer() {
    xfer.Stop();
    tf.status = Tf::kStatusReady | Tf::kStatusSeek;
    sense     = CompactFlashSense::kNone;
    power.MarkActivity();
}

void CompactFlashAtaCore::SaveState(StateWriter& w) {
    tf.SaveState(w);
    xfer.SaveState(w);
    w.Write("sectors_left", sectors_left);
    w.Write<uint8_t>("intrq", intrq ? 1u : 0u);
    w.Write<uint8_t>("multiple", multiple ? 1u : 0u);
    w.Write("sense", sense);
    w.Write<uint8_t>("soft_reset_defaults", soft_reset_restores_defaults ? 1u : 0u);
    power.SaveState(w);
}

void CompactFlashAtaCore::RestoreState(StateReader& r) {
    tf.RestoreState(r);
    xfer.RestoreState(r);
    r.Read("sectors_left", sectors_left);
    uint8_t saved_intrq = 0;
    r.Read("intrq", saved_intrq);
    intrq = saved_intrq != 0u;
    uint8_t saved_multiple = 0;
    r.Read("multiple", saved_multiple);
    multiple = saved_multiple != 0u;
    r.Read("sense", sense);
    uint8_t saved_defaults = 0;
    r.Read("soft_reset_defaults", saved_defaults);
    soft_reset_restores_defaults = saved_defaults != 0u;
    power.RestoreState(r);
}

}
