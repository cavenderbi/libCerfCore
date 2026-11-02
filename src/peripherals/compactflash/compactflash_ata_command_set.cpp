#include "compactflash_ata_command_set.h"

#include "compactflash_ata_commands.h"
#include "compactflash_ata_core.h"
#include "compactflash_identify.h"

namespace cerfcore {

namespace {

using Tf = CompactFlashTaskFile;

constexpr uint8_t kLongSectorEcc        = 0x00u;
constexpr uint8_t kPowerModeSleep       = 0x00u;
constexpr uint8_t kPowerModeIdle        = 0xFFu;
constexpr uint8_t kMultipleBlockSectors = 1u;

}

void CompactFlashAtaCommandSet::Execute(uint8_t command, bool was_sleeping) {
    if (ExecutePowerCommand(command, was_sleeping))
        return;
    if (ExecuteSetupCommand(command))
        return;
    ExecuteDataCommand(command);
}

void CompactFlashAtaCommandSet::FinishWithInterrupt() {
    core_.FinishTransfer();
    core_.IrqAssert();
}

void CompactFlashAtaCommandSet::OnReadBufferEnd() {
    if (core_.xfer.Kind() != CompactFlashTransfer::kReadSectors) {
        core_.FinishTransfer();
        return;
    }
    --core_.sectors_left;
    core_.tf.sect_cnt = static_cast<uint8_t>(core_.sectors_left);
    if (core_.sectors_left == 0u) {
        core_.FinishTransfer();
        return;
    }
    core_.tf.Advance();
    LoadReadSector();
}

void CompactFlashAtaCommandSet::OnWriteBufferFull() {
    switch (core_.xfer.Kind()) {
        case CompactFlashTransfer::kWriteSectors:
            WriteSector(false);
            return;
        case CompactFlashTransfer::kWriteVerify:
            WriteSector(true);
            return;
        case CompactFlashTransfer::kWriteBuffer:
            FinishWithInterrupt();
            return;
        case CompactFlashTransfer::kWriteLong:
            if (core_.Succeeded(core_.media.Write(core_.tf, core_.xfer.Buffer().data(), false)))
                FinishWithInterrupt();
            return;
        case CompactFlashTransfer::kFormatTrack:
            if (core_.Succeeded(core_.media.Format(core_.tf)))
                FinishWithInterrupt();
            return;
        case CompactFlashTransfer::kNone:
        case CompactFlashTransfer::kReadSectors:
        case CompactFlashTransfer::kReadOnce:
        case CompactFlashTransfer::kReadLong:
            return;
    }
}

void CompactFlashAtaCommandSet::WriteSector(bool verify) {
    core_.tf.sect_cnt = static_cast<uint8_t>(core_.sectors_left);
    if (!core_.Succeeded(core_.media.Write(core_.tf, core_.xfer.Buffer().data(), verify)))
        return;
    if (--core_.sectors_left == 0u) {
        core_.tf.sect_cnt = 0;
        FinishWithInterrupt();
        return;
    }
    core_.tf.sect_cnt = static_cast<uint8_t>(core_.sectors_left);
    core_.tf.Advance();
    core_.xfer.Start(core_.xfer.Kind());
    core_.power.MarkActivity();
    core_.IrqAssert();
}

bool CompactFlashAtaCommandSet::ExecutePowerCommand(uint8_t command, bool was_sleeping) {
    using namespace CompactFlashCommand;
    switch (command) {
        case kCheckPowerMode:
        case kCheckPowerModeAlt:
            core_.tf.sect_cnt = was_sleeping ? kPowerModeSleep : kPowerModeIdle;
            core_.Complete();
            return true;
        case kIdle:
        case kIdleAlt:
            core_.power.SetIdleTimer(core_.tf.sect_cnt);
            core_.Complete();
            return true;
        case kIdleImmediate:
        case kIdleImmediateAlt:
            core_.Complete();
            return true;
        case kStandby:
        case kStandbyAlt:
        case kStandbyImmediate:
        case kStandbyImmediateAlt:
        case kSleep:
        case kSleepAlt:
            core_.Complete();
            core_.power.Sleep();
            return true;
        default:
            return false;
    }
}

bool CompactFlashAtaCommandSet::ExecuteSetupCommand(uint8_t command) {
    using namespace CompactFlashCommand;
    switch (command) {
        case kNop:
            core_.Fail(Tf::kErrorAbort, CompactFlashSense::kAborted);
            return true;
        case kRequestSense:
            RequestSense();
            return true;
        case kDiagnostic:
            core_.Complete();
            core_.tf.error = Tf::kDiagnosticNoError;
            core_.sense    = CompactFlashSense::kSelfTestOk;
            return true;
        case kInitializeParameters:
            InitializeParameters();
            return true;
        case kSetMultiple:
            SetMultiple();
            return true;
        case kFlushCache:
            core_.Complete();
            return true;
        case kSetFeatures:
            SetFeatures();
            return true;
        case kWearLevel:
            core_.tf.sect_cnt = 0;
            core_.Complete();
            return true;
        default:
            return false;
    }
}

void CompactFlashAtaCommandSet::RequestSense() {
    const uint8_t code = core_.sense;
    core_.Complete();
    core_.tf.error = code;
}

void CompactFlashAtaCommandSet::InitializeParameters() {
    core_.tf.geometry = CompactFlashGeometry{(core_.tf.drv_head & 0x0Fu) + 1u, core_.tf.sect_cnt};
    core_.Complete();
}

void CompactFlashAtaCommandSet::SetMultiple() {
    core_.multiple = core_.tf.sect_cnt == kMultipleBlockSectors;
    if (core_.tf.sect_cnt == 0u || core_.multiple) {
        core_.Complete();
        return;
    }
    core_.Fail(Tf::kErrorAbort, CompactFlashSense::kAborted);
}

void CompactFlashAtaCommandSet::SetFeatures() {
    switch (CompactFlashCommand::ClassifyFeature(core_.tf.feature, core_.tf.sect_cnt)) {
        case CompactFlashFeatureResult::kAbort:
            core_.Fail(Tf::kErrorAbort, CompactFlashSense::kAborted);
            return;
        case CompactFlashFeatureResult::kKeepSettingsOnSoftReset:
            core_.soft_reset_restores_defaults = false;
            break;
        case CompactFlashFeatureResult::kDefaultsOnSoftReset:
            core_.soft_reset_restores_defaults = true;
            break;
        case CompactFlashFeatureResult::kAccept:
            break;
    }
    core_.Complete();
}

void CompactFlashAtaCommandSet::ReadSectors() {
    core_.sectors_left = core_.tf.SectorCount();
    LoadReadSector();
}

void CompactFlashAtaCommandSet::LoadReadSector() {
    if (core_.Succeeded(core_.media.Read(core_.tf, core_.xfer.Buffer().data())))
        core_.BeginRead(CompactFlashTransfer::kReadSectors);
}

void CompactFlashAtaCommandSet::ReadLong() {
    if (!core_.Succeeded(core_.media.Read(core_.tf, core_.xfer.Buffer().data())))
        return;
    core_.xfer.FillEcc(kLongSectorEcc);
    core_.BeginRead(CompactFlashTransfer::kReadLong);
}

void CompactFlashAtaCommandSet::WalkSectors() {
    core_.sectors_left = core_.tf.SectorCount();
    for (;;) {
        core_.tf.sect_cnt = static_cast<uint8_t>(core_.sectors_left);
        if (!core_.Succeeded(core_.media.Check(core_.tf)))
            return;
        if (--core_.sectors_left == 0u)
            break;
        core_.tf.Advance();
    }
    core_.tf.sect_cnt = 0;
    core_.Complete();
}

void CompactFlashAtaCommandSet::Identify() {
    BuildCompactFlashIdentify(core_.xfer.Buffer(), core_.tf, core_.media.TotalSectors(), core_.multiple);
    core_.BeginRead(CompactFlashTransfer::kReadOnce);
}

void CompactFlashAtaCommandSet::TranslateSector() {
    if (core_.Succeeded(core_.media.Translate(core_.tf, core_.xfer.Buffer().data())))
        core_.BeginRead(CompactFlashTransfer::kReadOnce);
}

void CompactFlashAtaCommandSet::ExecuteDataCommand(uint8_t command) {
    using namespace CompactFlashCommand;
    if ((command & kFamilyMask) == kRecalibrate) {
        core_.Complete();
        return;
    }
    if ((command & kFamilyMask) == kSeek) {
        if (core_.Succeeded(core_.media.Seek(core_.tf))) core_.Complete();
        return;
    }
    const uint32_t sectors = core_.tf.SectorCount();
    switch (command) {
        case kReadSectors:
        case kReadSectorsAlt:
            ReadSectors();
            return;
        case kReadMultiple:
            if (core_.multiple) ReadSectors();
            else                core_.Fail(Tf::kErrorAbort, CompactFlashSense::kAborted);
            return;
        case kReadLong:
        case kReadLongAlt:
            ReadLong();
            return;
        case kReadVerify:
        case kReadVerifyAlt:
        case kEraseSectors:
            WalkSectors();
            return;
        case kReadBuffer:
            core_.BeginRead(CompactFlashTransfer::kReadOnce);
            return;
        case kIdentify:
            Identify();
            return;
        case kTranslateSector:
            TranslateSector();
            return;
        case kWriteSectors:
        case kWriteSectorsAlt:
        case kWriteSectorsNoErase:
            core_.BeginWrite(CompactFlashTransfer::kWriteSectors, sectors);
            return;
        case kWriteMultiple:
        case kWriteMultipleNoErase:
            if (core_.multiple) core_.BeginWrite(CompactFlashTransfer::kWriteSectors, sectors);
            else                core_.Fail(Tf::kErrorAbort, CompactFlashSense::kAborted);
            return;
        case kWriteVerify:
            core_.BeginWrite(CompactFlashTransfer::kWriteVerify, sectors);
            return;
        case kWriteBuffer:
            core_.BeginWrite(CompactFlashTransfer::kWriteBuffer, 1u);
            return;
        case kWriteLong:
        case kWriteLongAlt:
            core_.BeginWrite(CompactFlashTransfer::kWriteLong, 1u);
            return;
        case kFormatTrack:
            core_.BeginWrite(CompactFlashTransfer::kFormatTrack, 1u);
            return;
        default:
            core_.Fail(Tf::kErrorAbort, CompactFlashSense::kInvalidCommand);
            return;
    }
}

}
