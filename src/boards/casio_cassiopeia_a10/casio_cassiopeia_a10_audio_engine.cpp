#include <cerfcore/peripherals/peripheral_base.h>

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/instance_threads.h>
#include <cerfcore/cpu/emulated_memory.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>
#include "../../execution/guest_cycle_clock.h"
#include "../../host/audio_activity.h"
#include "../../host/paced_wave_out.h"
#include "../../peripherals/peripheral_dispatcher.h"
#include "../../socs/oscillator_ticks.h"
#include "casio_cassiopeia_a10_audio_control.h"
#include "casio_cassiopeia_a10_audio_layout.h"
#include "casio_cassiopeia_a10_audio_status_clear.h"
#include "casio_cassiopeia_a10_family_id.h"
#include "casio_cassiopeia_a10_microphone.h"
#include "casio_cassiopeia_a10_request_lines.h"
#include "casio_cassiopeia_a10_speaker.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace cerfcore {

namespace {

constexpr uint32_t kRun            = 0x10000160u;
constexpr uint32_t kStatus         = 0x10000164u;
constexpr uint32_t kStartLow       = 0x10000170u;
constexpr uint32_t kStartHigh      = 0x10000172u;
constexpr uint32_t kEndLow         = 0x10000174u;
constexpr uint32_t kNextStartLow   = 0x10000178u;
constexpr uint32_t kNextStartHigh  = 0x1000017Au;
constexpr uint32_t kNextEndLow     = 0x1000017Cu;
constexpr uint32_t kBlockEnd       = 0x1000017Eu;

constexpr uint16_t kPlay       = 0x0001u;
constexpr uint16_t kRecord     = 0x0002u;
constexpr uint16_t kSwitched   = 0x0001u;
constexpr uint16_t kEnded      = 0x0002u;
constexpr uint16_t kStatusBits = kSwitched | kEnded;

constexpr uint16_t kRateField = 0x0030u;
constexpr uint32_t kRateShift = 4u;
constexpr uint32_t kNoRate    = 0u;
constexpr uint32_t kRates[]   = {8000u, 22050u, kNoRate, 11025u};

constexpr uint32_t kPhysicalMask = 0x1FFFFFFFu;
constexpr uint32_t kSampleBytes  = 2u;
constexpr uint32_t kFeedFrames   = 256u;
constexpr uint16_t kChannels     = 1u;
constexpr uint16_t kSampleBits   = 16u;

constexpr char kLogTag[] = "CasioCassiopeiaA10Audio";

struct Buffer {
    uint16_t start_low  = 0u;
    uint16_t start_high = 0u;
    uint16_t end_low    = 0u;

    uint32_t Start(uint32_t unit_shift) const {
        return ((static_cast<uint32_t>(start_high) << 16) | start_low) << unit_shift;
    }
    uint32_t End(uint32_t unit_shift) const {
        return (((static_cast<uint32_t>(start_high) << 16) | end_low) << unit_shift) | ((1u << unit_shift) - 1u);
    }
};

class CasioCassiopeiaA10AudioEngine : public Peripheral {
public:
    using Peripheral::Peripheral;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10;
    }

    void OnReady() override {
        control_      = &emu_.Get<CasioCassiopeiaA10AudioControl>();
        requests_     = &emu_.Get<CasioCassiopeiaA10RequestLines>();
        status_clear_ = &emu_.Get<CasioCassiopeiaA10AudioStatusClear>();
        speaker_      = &emu_.Get<CasioCassiopeiaA10Speaker>();
        microphone_   = &emu_.Get<CasioCassiopeiaA10Microphone>();
        memory_       = &emu_.Get<EmulatedMemory>();
        clock_        = &emu_.Get<GuestCycleClock>();
        const CasioCassiopeiaA10AudioLayout& layout = emu_.Get<CasioCassiopeiaA10AudioLayout>();
        unit_shift_   = layout.UnitShift();
        slot_bytes_   = layout.SlotBytes();
        event_        = clock_->Add([this] { OnEvent(); });
        ticks_.Attach(kRates[0], 1u);
        clock_->RegisterRateListener([this] {
            ticks_.Rescale();
            Rearm();
        });
        emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) { Reset(); });
        out_.Start(emu_.Get<Fatal>(), emu_.Get<InstanceThreads>(), emu_.TryGet<AudioOutputs>(), kLogTag, 0u, kChannels,
                   kSampleBits);
        activity_ = &emu_.Get<AudioActivity>();
        activity_->NotePresent();
        emu_.Get<PeripheralDispatcher>().Register(this);
    }

    void OnShutdown() override { out_.Stop(); }

    uint32_t MmioBase() const override { return kRun; }
    uint32_t MmioSize() const override { return kBlockEnd - kRun; }

    uint16_t ReadHalf(uint32_t addr) override {
        if (addr == kRun) return run_;
        if (addr != kStatus) HaltUnsupportedAccess("ReadHalf", addr, 0u);
        const uint16_t status = status_;
        if (status_clear_->ClearsOnRead()) {
            status_ = 0u;
            DriveLine();
        }
        return status;
    }

    void WriteHalf(uint32_t addr, uint16_t value) override {
        switch (addr) {
        case kRun: {
            const bool record = (value & kRecord) != 0u;
            if ((value & ~(kPlay | kRecord)) != 0u || value == (kPlay | kRecord)) {
                HaltUnsupportedAccess("WriteHalf", addr, value);
            }
            run_ = value;
            if (running_ && (value == 0u || record != recording_)) Stop();
            if (value != 0u && !running_) Start(record);
            return;
        }
        case kStatus:
            if ((value & ~kStatusBits) != 0u || status_clear_->ClearsOnRead()) {
                HaltUnsupportedAccess("WriteHalf", addr, value);
            }
            status_ = static_cast<uint16_t>(status_ & ~value);
            DriveLine();
            return;
        case kStartLow:
        case kStartHigh:
        case kEndLow:
            if (running_) HaltUnsupportedAccess("WriteHalf", addr, value);
            Field(current_, addr - kStartLow) = value;
            return;
        case kNextStartLow:
        case kNextStartHigh:
            Field(next_, addr - kNextStartLow) = value;
            return;
        case kNextEndLow:
            next_.end_low = value;
            next_armed_   = true;
            return;
        default:
            HaltUnsupportedAccess("WriteHalf", addr, value);
        }
    }

    void SaveState(StateWriter& w) override {
        w.Write("run", run_);
        w.Write("status", status_);
        SaveBuffer(w, "current_", current_);
        SaveBuffer(w, "next_", next_);
        w.Write("next_armed", next_armed_);
        w.Write("running", running_);
        w.Write("recording", recording_);
        w.Write("rate_hz", rate_hz_);
        w.Write("capture_hz", capture_hz_);
        w.Write("end_tick", end_tick_);
        w.Write("cursor", cursor_);
        w.Write("unread_frames", unread_frames_);
        w.Write("feed_tick", feed_tick_);
        ticks_.Save(w, "ticks_");
    }

    void RestoreState(StateReader& r) override {
        r.Read("run", run_);
        r.Read("status", status_);
        RestoreBuffer(r, "current_", current_);
        RestoreBuffer(r, "next_", next_);
        r.Read("next_armed", next_armed_);
        r.Read("running", running_);
        r.Read("recording", recording_);
        r.Read("rate_hz", rate_hz_);
        r.Read("capture_hz", capture_hz_);
        r.Read("end_tick", end_tick_);
        r.Read("cursor", cursor_);
        r.Read("unread_frames", unread_frames_);
        r.Read("feed_tick", feed_tick_);
        ticks_.Restore(r, "ticks_");
    }

    void PostRestore() override {
        out_.StopAudioOut();
        streaming_ = false;
        if (rate_hz_ != 0u) out_.SetFormat(rate_hz_, kChannels, kSampleBits);
        if (running_ && recording_) microphone_->Open(capture_hz_);
        Rearm();
        DriveLine();
    }

private:
    static uint16_t& Field(Buffer& buffer, uint32_t offset) {
        if (offset == 0u) return buffer.start_low;
        if (offset == 2u) return buffer.start_high;
        return buffer.end_low;
    }

    static void SaveBuffer(StateWriter& w, const char* prefix, const Buffer& buffer) {
        const std::string p = prefix;
        w.Write((p + "start_low").c_str(), buffer.start_low);
        w.Write((p + "start_high").c_str(), buffer.start_high);
        w.Write((p + "end_low").c_str(), buffer.end_low);
    }

    static void RestoreBuffer(StateReader& r, const char* prefix, Buffer& buffer) {
        const std::string p = prefix;
        r.Read((p + "start_low").c_str(), buffer.start_low);
        r.Read((p + "start_high").c_str(), buffer.start_high);
        r.Read((p + "end_low").c_str(), buffer.end_low);
    }

    uint32_t Frames(const Buffer& buffer) {
        const uint32_t start = buffer.Start(unit_shift_);
        const uint32_t end   = buffer.End(unit_shift_);
        const uint32_t bytes = end >= start ? end - start + 1u : 0u;
        if (bytes == 0u || bytes % slot_bytes_ != 0u) {
            emu_.Get<Fatal>().Die("CasioCassiopeiaA10AudioEngine: buffer 0x%08X-0x%08X", start, end);
        }
        return bytes / slot_bytes_;
    }

    void Start(bool record) {
        const uint32_t code = (control_->Value() & kRateField) >> kRateShift;
        if (kRates[code] == kNoRate) {
            emu_.Get<Fatal>().Die("CasioCassiopeiaA10AudioEngine: transfer starts with rate code %u", code);
        }
        recording_ = record;
        if (record) {
            capture_hz_ = kRates[code];
            microphone_->Open(capture_hz_);
        } else if (kRates[code] != rate_hz_) {
            rate_hz_ = kRates[code];
            out_.SetFormat(rate_hz_, kChannels, kSampleBits);
        }
        ticks_.SetOscRate(kRates[code], 1u);
        running_  = true;
        end_tick_ = ticks_.Now();
        Begin(current_);
    }

    void Begin(const Buffer& buffer) {
        const uint32_t frames = Frames(buffer);
        cursor_               = buffer.Start(unit_shift_) & kPhysicalMask;
        unread_frames_        = frames;
        feed_tick_            = end_tick_;
        end_tick_ += frames;
        if (recording_) {
            Rearm();
            return;
        }
        if (!streaming_) {
            out_.BeginAudioOut();
            streaming_ = true;
        }
        Feed();
    }

    uint32_t TakeSlots() {
        const uint32_t frames = std::min(unread_frames_, kFeedFrames);
        const uint32_t bytes  = frames * slot_bytes_;
        slots_.resize(bytes);
        if (memory_->TryCopyOut(cursor_, slots_.data(), bytes) != bytes) {
            emu_.Get<Fatal>().Die("CasioCassiopeiaA10AudioEngine: samples at PA 0x%08X are not in guest memory", cursor_);
        }
        return frames;
    }

    void Advance(uint32_t frames) {
        cursor_ += frames * slot_bytes_;
        unread_frames_ -= frames;
        feed_tick_ += frames;
    }

    void Feed() {
        const uint32_t frames  = TakeSlots();
        const bool     audible = speaker_->On();
        samples_.assign(static_cast<size_t>(frames) * kSampleBytes, uint8_t{0u});
        if (audible) {
            for (uint32_t i = 0; i < frames; ++i) {
                std::copy_n(slots_.data() + static_cast<size_t>(i) * slot_bytes_, kSampleBytes,
                            samples_.data() + static_cast<size_t>(i) * kSampleBytes);
            }
            activity_->MarkTx();
        }
        out_.QueueOutputInHostBlocks(samples_.data(), static_cast<uint32_t>(samples_.size()));
        Advance(frames);
        Rearm();
    }

    void Record() {
        const uint32_t frames = TakeSlots();
        microphone_->Fill(slots_.data(), frames, slot_bytes_);
        const uint32_t bytes = frames * slot_bytes_;
        if (memory_->TryCopyIn(cursor_, slots_.data(), bytes) != bytes) {
            emu_.Get<Fatal>().Die("CasioCassiopeiaA10AudioEngine: samples at PA 0x%08X are not in guest memory", cursor_);
        }
        Advance(frames);
    }

    void OnEvent() {
        if (!running_) return;
        if (unread_frames_ != 0u) {
            if (!recording_) {
                Feed();
                return;
            }
            Record();
            if (unread_frames_ != 0u) {
                Rearm();
                return;
            }
        }
        if (next_armed_) {
            current_    = next_;
            next_armed_ = false;
            status_     = static_cast<uint16_t>(status_ | kSwitched);
            Begin(current_);
        } else {
            running_ = false;
            status_  = static_cast<uint16_t>(status_ | kEnded);
            EndStream();
        }
        DriveLine();
    }

    void Stop() {
        running_       = false;
        recording_     = false;
        unread_frames_ = 0u;
        clock_->Disarm(event_);
        if (streaming_) out_.StopAudioOut();
        streaming_ = false;
    }

    void EndStream() {
        if (streaming_) out_.FinishAudioOut();
        streaming_ = false;
    }

    void Rearm() {
        if (!running_) return;
        if (unread_frames_ == 0u) {
            ticks_.ArmAt(event_, end_tick_);
        } else if (recording_) {
            ticks_.ArmAt(event_, feed_tick_ + std::min(unread_frames_, kFeedFrames));
        } else {
            ticks_.ArmAt(event_, feed_tick_);
        }
    }

    void DriveLine() {
        requests_->Drive(CasioCassiopeiaA10Request::kAudioSwitched, (status_ & kSwitched) != 0u);
        requests_->Drive(CasioCassiopeiaA10Request::kAudioEnded, (status_ & kEnded) != 0u);
    }

    void Reset() {
        Stop();
        run_        = 0u;
        status_     = 0u;
        current_    = Buffer{};
        next_       = Buffer{};
        next_armed_ = false;
        DriveLine();
    }

    CasioCassiopeiaA10AudioControl*     control_      = nullptr;
    CasioCassiopeiaA10RequestLines*     requests_     = nullptr;
    CasioCassiopeiaA10AudioStatusClear* status_clear_ = nullptr;
    AudioActivity*                      activity_     = nullptr;
    const CasioCassiopeiaA10Speaker*    speaker_      = nullptr;
    CasioCassiopeiaA10Microphone*       microphone_   = nullptr;
    EmulatedMemory*                     memory_       = nullptr;
    GuestCycleClock*                    clock_        = nullptr;
    GuestCycleClock::Event*             event_        = nullptr;
    uint32_t                            unit_shift_   = 0u;
    uint32_t                            slot_bytes_   = 0u;
    OscillatorTicks                     ticks_{emu_, false};
    PacedWaveOut                        out_;
    std::vector<uint8_t>                slots_;
    std::vector<uint8_t>                samples_;
    Buffer                              current_;
    Buffer                              next_;
    uint16_t                            run_           = 0u;
    uint16_t                            status_        = 0u;
    uint32_t                            rate_hz_       = 0u;
    uint32_t                            capture_hz_    = 0u;
    uint64_t                            end_tick_      = 0u;
    uint64_t                            feed_tick_     = 0u;
    uint32_t                            cursor_        = 0u;
    uint32_t                            unread_frames_ = 0u;
    bool                                next_armed_    = false;
    bool                                running_       = false;
    bool                                recording_     = false;
    bool                                streaming_     = false;
};

}

REGISTER_SERVICE(CasioCassiopeiaA10AudioEngine);

}
