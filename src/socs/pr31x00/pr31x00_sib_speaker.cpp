#include "pr31x00_sib_speaker.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/instance_threads.h>
#include "../../host/audio_activity.h"
#include "pr31x00_sib_codec.h"
#include "pr31500_id.h"
#include "pr31700_id.h"

namespace cerfcore {

bool Pr31x00SibSpeaker::ShouldRegister() {
    const std::string_view soc = emu_.Get<BoardContext>().GetSocId();
    return soc == SocId::Pr31500 || soc == SocId::Pr31700;
}

void Pr31x00SibSpeaker::OnReady() {
    codec_ = &emu_.Get<Pr31x00SibCodec>();
    paced_.Start(emu_.Get<Fatal>(), emu_.Get<InstanceThreads>(), emu_.TryGet<AudioOutputs>(), "SibAudio", 0, 1, 16);
    AudioActivity& activity = emu_.Get<AudioActivity>();
    activity.NotePresent();
    buf_.Attach(paced_, activity);
}

void Pr31x00SibSpeaker::OnShutdown() { paced_.Stop(); }

void Pr31x00SibSpeaker::SetFormat(uint32_t hz, uint16_t channels) {
    paced_.SetFormat(hz, channels, 16);
}

void Pr31x00SibSpeaker::Begin() { paced_.BeginAudioOut(); }

void Pr31x00SibSpeaker::Finish() { paced_.FinishAudioOut(); }

void Pr31x00SibSpeaker::Abort() { paced_.StopAudioOut(); }

void Pr31x00SibSpeaker::AddWord(uint32_t hold, bool sixteen_bit) {
    const uint32_t parts   = sixteen_bit ? 2u : 4u;
    const uint32_t width   = 32u / parts;
    const bool     audible = codec_->AudioOutputAudible();
    for (uint32_t i = 0; i < parts; ++i) {
        const uint32_t field  = (hold >> (32u - width * (i + 1u))) & ((1u << width) - 1u);
        const uint32_t sample = !audible ? 0u : sixteen_bit ? field : field << 8;
        buf_.Append16(static_cast<uint16_t>(sample));
    }
}

void Pr31x00SibSpeaker::Flush() { buf_.Push(); }

REGISTER_SERVICE(Pr31x00SibSpeaker);

}
