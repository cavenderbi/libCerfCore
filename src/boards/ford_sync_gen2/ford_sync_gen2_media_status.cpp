#include "ford_sync_gen2_media_status.h"
#include <cerfcore/boards/board_context.h>
#include "ford_sync_gen2_family_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/state/state_stream.h>

namespace cerfcore {

REGISTER_SERVICE(FordSyncGen2MediaStatus);

bool FordSyncGen2MediaStatus::ShouldRegister() {
    auto* board = emu_.TryGet<BoardContext>();
    return board && board->GetBoardFamilyId() == BoardFamilyId::FordSyncGen2;
}

void FordSyncGen2MediaStatus::OnReady() {
    emu_.Get<FordSyncGen2IlpChannel>().RegisterDevice({
        "media-status", [](bool) {}, {},
        [](uint32_t id) { return id >= kFirst && id < kFirst + 4; },
        [this](const std::vector<Channel::Write>& writes) { return Apply(writes); },
        [this](StateWriter& w) {
            const auto s = Snapshot(); w.Write("valid", s.valid);
            for (auto value : s.values) w.Write("value", value);
        },
        [this](StateReader& r) {
            Status s; r.Read("valid", s.valid);
            for (auto& value : s.values) r.Read("value", value);
            if (s.valid > 15 || s.values[0] > 65535 ||
                s.values[1] > 65535 || s.values[2] > 65535) {
                r.Reject("[AHU] invalid media-status snapshot");
            }
            std::lock_guard lock(mutex_); status_ = s;
        },
        [this] { std::lock_guard lock(mutex_); status_ = {}; }
    });
}

FordSyncGen2MediaProgress FordSyncGen2MediaStatus::ReadProgress() const {
    const auto s = Snapshot();
    return {FordSyncGen2MediaProgress::Decode(s.values[0], (s.valid & 1) != 0),
        FordSyncGen2MediaProgress::Decode(s.values[1], (s.valid & 2) != 0)};
}

FordSyncGen2IlpChannel::Result FordSyncGen2MediaStatus::Apply(const std::vector<Channel::Write>& writes) {
    if (writes.empty()) return Channel::Result::Invalid;
    std::lock_guard lock(mutex_);
    auto next = status_;
    unsigned seen = 0;
    for (const auto& w : writes) {
        if (w.id < kFirst || w.id >= kFirst + 4) return Channel::Result::Invalid;
        const auto index = w.id - kFirst;
        if ((seen & (1u << index)) || w.value > (index == 3 ? UINT32_MAX : 65535u))
            return Channel::Result::Invalid;
        seen |= 1u << index;
        next.values[index] = static_cast<uint32_t>(w.value);
        next.valid |= static_cast<uint8_t>(1u << index);
    }
    status_ = next;
    return Channel::Result::Accepted;
}

}
