#pragma once

#include "../core/service.h"
#include "machine_object.h"

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace cerfcore {

class StateReader;
class StateWriter;

enum class MachineEventKind : uint8_t { kAdded, kChanged, kInserted, kEjected, kFailed };

struct MachineEvent {
    MachineEventKind kind   = MachineEventKind::kChanged;
    MachineObject*   object = nullptr;
    MachineObject*   parent = nullptr;
    std::string      reason;
    int64_t          code = 0;
    std::string      subject;
};

class MachineObjectTree : public Service {
public:
    using Service::Service;
    using Listener = std::function<void(const MachineEvent&)>;

    void Add(MachineObject* object, MachineObject* parent = nullptr);

    std::vector<MachineObject*> Roots();
    std::vector<MachineObject*> Children(const MachineObject* parent);
    MachineObject*              Parent(const MachineObject* object);
    std::vector<std::string>    PathOf(const MachineObject* object);
    MachineObject*              Find(const std::vector<std::string>& path);

    void Subscribe(Listener listener);

    void SaveState(StateWriter& w);
    void RestoreState(StateReader& r);

private:
    friend class MachineObject;
    friend class MachineSocket;
    friend class Hibernation;
    friend class GuestPowerNotifier;

    void NotifyChanged(MachineObject* object);
    void NotifyInserted(MachineObject* socket);
    void NotifyEjected(MachineObject* socket);
    void NotifyFailed(MachineObject* object, std::string reason, int64_t code, std::string subject);
    void RefreshAll();

    struct Node {
        MachineObject* object = nullptr;
        MachineObject* parent = nullptr;
    };

    std::vector<std::pair<std::string, MachineObject*>> KeyedObjects();

    void                     Publish(const MachineEvent& event);
    std::vector<std::string> PathOfLocked(const MachineObject* object) const;
    MachineObject*           ParentLocked(const MachineObject* object) const;
    std::vector<Node>        PreorderLocked() const;
    void                     AppendSubtreeLocked(const MachineObject* parent, std::vector<Node>& out) const;

    std::mutex            mtx_;
    std::vector<Node>     nodes_;
    std::mutex            listeners_mtx_;
    std::vector<Listener> listeners_;
};

}
