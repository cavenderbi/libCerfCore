#pragma once

#include <cerfcore/core/service.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

namespace cerfcore {

class S3C2410HeldLevelSource {
public:
    virtual ~S3C2410HeldLevelSource() = default;

    virtual bool LevelAsserted(int source_bit) = 0;
};

class S3C2410HeldSourceLevels : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;

    void RegisterMain(int main_source_bit, S3C2410HeldLevelSource* source);
    void RegisterSub(int main_source_bit, int sub_source_bit, S3C2410HeldLevelSource* source);

    void ReassertStillHeld(uint32_t cleared_main, uint32_t cleared_sub);

private:
    static constexpr int kNoSubSource = -1;

    struct Entry {
        int                     main_bit;
        int                     sub_bit;
        S3C2410HeldLevelSource* source;
    };

    void Add(const Entry& entry);

    std::atomic<uint32_t> main_mask_{0};
    std::atomic<uint32_t> sub_mask_{0};
    std::mutex            mutex_;
    std::vector<Entry>    entries_;
};

}
