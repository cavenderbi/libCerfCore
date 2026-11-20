#pragma once

#include "../core/callback_list.h"
#include "../core/service.h"

#include <cstdint>
#include <functional>
#include <limits>
#include <mutex>
#include <string>
#include <vector>

namespace cerfcore {

class MachineObject;

enum class MachineRequestKind : uint8_t { kPath, kDocuments, kSize, kOutputPath };

struct MachineRequest {
    uint64_t                 serial = 0;
    MachineRequestKind       kind   = MachineRequestKind::kPath;
    std::string              id;
    MachineObject*           object = nullptr;
    std::vector<std::string> extensions;
    uint64_t                 minimum     = 0;
    uint64_t                 maximum     = std::numeric_limits<uint64_t>::max();
    uint64_t                 recommended = 0;
    std::string              recommended_path;
};

struct MachineReply {
    bool                     cancelled = true;
    std::string              path;
    std::vector<std::string> documents;
    uint64_t                 size = 0;
};

class MachineRequests : public Service {
public:
    using Service::Service;
    using Answer          = std::function<void(const MachineReply&)>;
    using Listener        = std::function<void(const MachineRequest&)>;
    using SettledListener = std::function<void(uint64_t serial)>;

    void Ask(MachineRequest request, Answer answer);
    bool Reply(uint64_t serial, const MachineReply& reply);
    bool Pending(uint64_t serial);
    void Subscribe(Listener listener);
    void SubscribeSettled(SettledListener listener);

private:
    static bool Fits(const MachineRequest& request, const MachineReply& reply);

    struct Open {
        MachineRequest request;
        Answer         answer;
    };

    std::mutex                          mtx_;
    std::vector<Open>                   open_;
    uint64_t                            next_serial_ = 1;
    CallbackList<const MachineRequest&> listeners_;
    CallbackList<uint64_t>              settled_listeners_;
};

}
