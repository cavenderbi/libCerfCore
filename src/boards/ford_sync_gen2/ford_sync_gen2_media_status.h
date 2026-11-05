#pragma once

#include <cerfcore/core/service.h>
#include "ford_sync_gen2_ilp_channel.h"
#include <array>
#include <mutex>
#include <optional>
#include <string>

namespace cerfcore {

struct FordSyncGen2MediaProgress {
    std::optional<uint32_t> elapsed, duration;
    static std::optional<uint32_t> Decode(uint32_t value, bool reported) {
        return reported && value >= 100 ? std::optional<uint32_t>(value - 100) : std::nullopt;
    }
    static std::string Time(std::optional<uint32_t> seconds) {
        if (!seconds) return "--:--";
        return std::to_string(*seconds / 60) + ":" +
            (*seconds % 60 < 10 ? "0" : "") + std::to_string(*seconds % 60);
    }
    std::string Text() const { return Time(elapsed) + " / " + Time(duration); }
};

class FordSyncGen2MediaStatus : public Service {
public:
    using Service::Service;
    bool ShouldRegister() override;
    void OnReady() override;
    FordSyncGen2MediaProgress ReadProgress() const;
private:
    using Channel = FordSyncGen2IlpChannel;
    static constexpr uint32_t kFirst = 0x02000212;
    struct Status { std::array<uint32_t, 4> values{}; uint8_t valid = 0; };
    mutable std::mutex mutex_;
    Status status_;
    Status Snapshot() const { std::lock_guard lock(mutex_); return status_; }
    Channel::Result Apply(const std::vector<Channel::Write>& writes);
};

}
