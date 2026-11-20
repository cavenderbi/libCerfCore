#pragma once

#include "../core/service.h"
#include "../host/notification_stack.h"
#include "state_image_format.h"
#include "../core/callback_list.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

namespace cerfcore {

class StateWriter;
class StateReader;

/* Callers must run Save/Restore on a thread other than the JIT thread -
   JitRunner::Pause self-deadlocks if called from the JIT thread. */
class Hibernation : public Service {
public:
    using Service::Service;
    ~Hibernation() override;

    enum class RestoreOutcome { Refused, Resumed, MemoryOnly, MemoryAfterRefusal };

    struct RestoreResult {
        RestoreOutcome outcome = RestoreOutcome::Refused;
        std::string    reason;
    };

    std::string DefaultStatePath() const;

    bool Save(const std::string& path);
    bool Restore(const std::string& path, bool ram_only = false);

    void SaveAsync(const std::string& path, std::function<void(bool)> on_done = {});
    bool RestoreAsync(const std::string& path, bool ram_only = false,
                      std::function<void(const RestoreResult&)> on_done = {});

    bool SaveAfterHalt(const std::string& path, std::string& error);

    void RegisterPreSaveListener(std::function<void()> fn);
    void RegisterSavedListener(std::function<void(const std::string& path)> fn);
    void RegisterPostRestoreListener(std::function<void()> fn);
    void RegisterRomImage(uint64_t bytes);

    void OnShutdown() override;

private:
    struct ImageMatch {
        bool same_rom;
        bool same_peripherals;
    };

    std::string      DeviceDirFile(const char* name) const;
    StateImageHeader LiveHeader() const;
    bool             WriteImage(const std::string& path, std::string& error);
    void             ShowSavingCard(NotificationId id);
    void             SaveSection(StateWriter& w, StateSection section);
    void             RestoreSection(StateReader& r, StateSection section);
    ImageMatch       ReadHeader(StateReader& r);
    void             ApplyImage(StateReader& r);
    void             ApplyMemory(const std::string& path, bool write, bool with_flash);
    void             FillMemory(const std::string& path, bool with_flash);
    bool             ApplyOrRollBack(const std::string& rollback_path,
                                     const std::function<void()>& apply, std::string& refusal);
    RestoreResult    RestoreImage(const std::string& path, bool ram_only);
    RestoreOutcome   ApplyStateFile(const std::string& path, bool ram_only, std::string& reason);
    void             FinishRestore(RestoreOutcome outcome, const std::string& reason);
    void             RollBack(const std::string& rollback_path);
    void             RestorePeripherals(StateReader& r);
    void             RestorePresentation(StateReader& r);
    uint32_t         PeripheralLayoutSig() const;
    void     Progress(const char* fmt, ...);
    void     JoinWorker();
    bool     ClaimWorker();

    std::thread            worker_;
    std::atomic<bool>      busy_{false};
    std::atomic<bool>      applying_image_{false};

    CallbackList<>                     pre_save_listeners_;
    CallbackList<const std::string&>   saved_listeners_;
    CallbackList<>                     post_restore_listeners_;
    uint64_t                           registered_rom_bytes_ = 0;
};

}
