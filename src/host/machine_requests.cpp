#include <cerfcore/host/machine_requests.h>

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/log.h>

#include <algorithm>
#include <utility>

namespace cerfcore {

REGISTER_SERVICE(MachineRequests);

void MachineRequests::Ask(MachineRequest request, Answer answer) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        request.serial = next_serial_++;
        open_.push_back(Open{request, std::move(answer)});
    }
    listeners_.Call(request);
}

bool MachineRequests::Fits(const MachineRequest& request, const MachineReply& reply) {
    if (reply.cancelled) return true;
    switch (request.kind) {
        case MachineRequestKind::kPath:       return !reply.path.empty();
        case MachineRequestKind::kDocuments:  return !reply.documents.empty();
        case MachineRequestKind::kSize:       return reply.size >= request.minimum && reply.size <= request.maximum;
        case MachineRequestKind::kOutputPath: return !reply.path.empty();
    }
    return false;
}

bool MachineRequests::Reply(uint64_t serial, const MachineReply& reply) {
    Answer answer;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        const auto it = std::find_if(open_.begin(), open_.end(),
                                     [serial](const Open& o) { return o.request.serial == serial; });
        if (it == open_.end()) {
            LOG(Machine, "[MACHINE] refused reply to unknown request %llu\n", static_cast<unsigned long long>(serial));
            return false;
        }
        if (!Fits(it->request, reply)) {
            LOG(Machine, "[MACHINE] refused reply to request %llu (%s): outside its limits\n",
                static_cast<unsigned long long>(serial), it->request.id.c_str());
            return false;
        }
        answer = std::move(it->answer);
        open_.erase(it);
    }
    settled_listeners_.Call(serial);
    answer(reply);
    return true;
}

bool MachineRequests::Pending(uint64_t serial) {
    std::lock_guard<std::mutex> lk(mtx_);
    return std::any_of(open_.begin(), open_.end(), [serial](const Open& o) { return o.request.serial == serial; });
}

void MachineRequests::Subscribe(Listener listener) { listeners_.Add(std::move(listener)); }

void MachineRequests::SubscribeSettled(SettledListener listener) { settled_listeners_.Add(std::move(listener)); }

}
