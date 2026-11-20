#include <cerfcore/host/machine_object_tree.h>

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/utf16.h>
#include <cerfcore/state/state_stream.h>

#include <algorithm>

namespace cerfcore {

REGISTER_SERVICE(MachineObjectTree);

namespace {

std::string JoinPath(const std::vector<std::string>& path) {
    std::string joined;
    for (const std::string& part : path) {
        if (!joined.empty()) joined += '/';
        joined += part;
    }
    return joined;
}

}

void MachineObjectTree::Add(MachineObject* object, MachineObject* parent) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        nodes_.push_back(Node{object, parent});
    }
    object->tree_.store(this, std::memory_order_release);
    Publish(MachineEvent{MachineEventKind::kAdded, object, parent, {}, 0, {}});
}

std::vector<MachineObject*> MachineObjectTree::Roots() { return Children(nullptr); }

std::vector<MachineObject*> MachineObjectTree::Children(const MachineObject* parent) {
    std::vector<MachineObject*> out;
    std::lock_guard<std::mutex> lk(mtx_);
    for (const Node& n : nodes_)
        if (n.parent == parent) out.push_back(n.object);
    return out;
}

MachineObject* MachineObjectTree::Parent(const MachineObject* object) {
    std::lock_guard<std::mutex> lk(mtx_);
    return ParentLocked(object);
}

std::vector<std::string> MachineObjectTree::PathOf(const MachineObject* object) {
    std::lock_guard<std::mutex> lk(mtx_);
    return PathOfLocked(object);
}

MachineObject* MachineObjectTree::Find(const std::vector<std::string>& path) {
    std::lock_guard<std::mutex> lk(mtx_);
    MachineObject* at = nullptr;
    for (const std::string& id : path) {
        const auto child = std::find_if(nodes_.begin(), nodes_.end(), [at, &id](const Node& n) {
            return n.parent == at && n.object->ObjectId() == id;
        });
        if (child == nodes_.end()) return nullptr;
        at = child->object;
    }
    return at;
}

void MachineObjectTree::Subscribe(Listener listener) {
    std::lock_guard<std::mutex> lk(listeners_mtx_);
    listeners_.push_back(std::move(listener));
}

void MachineObjectTree::NotifyChanged(MachineObject* object) {
    Publish(MachineEvent{MachineEventKind::kChanged, object, Parent(object), {}, 0, {}});
}

void MachineObjectTree::NotifyInserted(MachineObject* socket) {
    Publish(MachineEvent{MachineEventKind::kInserted, socket, Parent(socket), {}, 0, {}});
}

void MachineObjectTree::NotifyEjected(MachineObject* socket) {
    Publish(MachineEvent{MachineEventKind::kEjected, socket, Parent(socket), {}, 0, {}});
}

void MachineObjectTree::NotifyFailed(MachineObject* object, std::string reason, int64_t code, std::string subject) {
    Publish(MachineEvent{MachineEventKind::kFailed, object, Parent(object), std::move(reason), code, std::move(subject)});
}

void MachineObjectTree::RefreshAll() {
    std::vector<MachineObject*> objects;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        for (const Node& n : nodes_) objects.push_back(n.object);
    }
    for (MachineObject* object : objects) object->Republish();
}

void MachineObjectTree::Publish(const MachineEvent& event) {
    std::vector<Listener> listeners;
    {
        std::lock_guard<std::mutex> lk(listeners_mtx_);
        listeners = listeners_;
    }
    for (const Listener& listener : listeners) listener(event);
}

MachineObject* MachineObjectTree::ParentLocked(const MachineObject* object) const {
    for (const Node& n : nodes_)
        if (n.object == object) return n.parent;
    return nullptr;
}

std::vector<std::string> MachineObjectTree::PathOfLocked(const MachineObject* object) const {
    std::vector<std::string> path;
    for (const MachineObject* at = object; at != nullptr; at = ParentLocked(at)) path.push_back(at->ObjectId());
    std::reverse(path.begin(), path.end());
    return path;
}

void MachineObjectTree::AppendSubtreeLocked(const MachineObject* parent, std::vector<Node>& out) const {
    for (const Node& n : nodes_) {
        if (n.parent != parent) continue;
        out.push_back(n);
        AppendSubtreeLocked(n.object, out);
    }
}

std::vector<MachineObjectTree::Node> MachineObjectTree::PreorderLocked() const {
    std::vector<Node> out;
    AppendSubtreeLocked(nullptr, out);
    return out;
}

std::vector<std::pair<std::string, MachineObject*>> MachineObjectTree::KeyedObjects() {
    std::vector<std::pair<std::string, MachineObject*>> entries;
    std::lock_guard<std::mutex> lk(mtx_);
    for (const Node& n : PreorderLocked()) entries.emplace_back(JoinPath(PathOfLocked(n.object)), n.object);
    return entries;
}

void MachineObjectTree::SaveState(StateWriter& w) {
    const auto entries = KeyedObjects();
    w.Write<uint32_t>("object_count", static_cast<uint32_t>(entries.size()));
    for (const auto& [key, object] : entries) {
        const std::u16string units = Utf8ToUtf16(key);
        w.Write<uint32_t>("path_count", static_cast<uint32_t>(units.size()));
        w.WriteBytes("path", units.data(), units.size() * sizeof(char16_t));
        w.BeginFrame(0);
        object->SaveObjectState(w);
        w.EndFrame();
    }
}

void MachineObjectTree::RestoreState(StateReader& r) {
    const auto entries = KeyedObjects();
    uint32_t   n       = 0;
    r.Read("object_count", n);
    if (n != entries.size())
        r.Reject("the image holds %u machine objects, this build has %zu", n, entries.size());
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t len = 0;
        r.Read("path_count", len);
        std::u16string units(len, u'\0');
        r.ReadBytes("path", units.data(), len * sizeof(char16_t));
        const std::string key = Utf16ToUtf8(units);
        if (const uint32_t id = r.EnterFrame(); id != 0u)
            r.Reject("machine object '%s' body in frame 0x%X, this build writes frame 0", key.c_str(), id);
        const auto found = std::find_if(entries.begin(), entries.end(),
                                        [&](const auto& entry) { return entry.first == key; });
        if (found == entries.end()) r.Reject("machine object '%s' is not in this build", key.c_str());
        found->second->RestoreObjectState(r);
        r.LeaveFrame();
    }
}

}
