#include "msm8255_oncrpc_codec.h"
#include "msm8255_rpc_server.h"
#include "msm8255_rpc_server_registry.h"
#include "msm8255_rpcrouter_wire.h"

#include <cerfcore/boards/board_context.h>
#include "msm8255_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/cpu/emulated_memory.h>
#include <cerfcore/socs/guest_cpu_reset.h>
#include <cerfcore/state/state_stream.h>

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kTimeProg = 0x30000048u;
constexpr uint32_t kTimeVers = 0x00040001u;
constexpr uint32_t kTimeCid  = 8u;

constexpr uint32_t kProcSetDefaultTime = 8u;
constexpr uint32_t kProcClientInit     = 0x12u;
constexpr uint32_t kProcRequestCb      = 0x17u;

constexpr uint32_t kClientTypeInternal = 3u;
constexpr uint32_t kProcessorApp1      = 2u;
constexpr uint32_t kErrNone            = 0u;

constexpr uint32_t kByteMax = 0xFFu;

constexpr uint32_t kIssuedClientId = 0u;

constexpr uint32_t kInitTypeOff      = kCallArgsOff + 0u;
constexpr uint32_t kInitIdPtrOff     = kCallArgsOff + 4u;
constexpr uint32_t kInitIdOff        = kCallArgsOff + 8u;
constexpr uint32_t kInitProcessorOff = kCallArgsOff + 12u;
constexpr uint32_t kInitCallBytes    = kPacmarkBytes + kCallArgsOff + 16u;

constexpr uint32_t kCbClientIdOff = kCallArgsOff + 0u;
constexpr uint32_t kCbIdOff       = kCallArgsOff + 4u;
constexpr uint32_t kCbCallBytes   = kPacmarkBytes + kCallArgsOff + 8u;

constexpr uint32_t kNoArgCallBytes = kPacmarkBytes + kCallArgsOff;

class Msm8255TimeRemoteServer : public Msm8255RpcServer {
public:
    using Msm8255RpcServer::Msm8255RpcServer;

    bool ShouldRegister() override {
        auto* bd = emu_.TryGet<BoardContext>();
        return bd && bd->GetSocId() == SocId::Msm8255;
    }

    void OnReady() override {
        emu_.Get<Msm8255RpcServerRegistry>().Register(this);
        emu_.Get<GuestCpuReset>().RegisterResetListener([this](ResetLineKind) {
            client_registered_   = 0u;
            callback_registered_ = 0u;
            callback_id_         = 0u;
        });
    }

    uint32_t ServerProg() const override { return kTimeProg; }
    uint32_t ServerVers() const override { return kTimeVers; }
    uint32_t ServerCid() const override { return kTimeCid; }

    uint32_t AnswerCall(uint32_t in_pa, uint32_t size, uint32_t out_pa,
                        uint32_t out_cap, uint32_t self_pid, uint32_t peer_pid,
                        uint32_t peer_cid) override;

    void SaveState(StateWriter& w) override {
        w.Write<uint32_t>("time_client_registered", client_registered_);
        w.Write<uint32_t>("time_callback_registered", callback_registered_);
        w.Write<uint32_t>("time_callback_id", callback_id_);
    }

    void RestoreState(StateReader& r) override {
        r.Read("time_client_registered", client_registered_);
        r.Read("time_callback_registered", callback_registered_);
        r.Read("time_callback_id", callback_id_);
    }

private:
    uint32_t AnswerSetDefaultTime(const Msm8255OncrpcCall& call, uint32_t size,
                                  uint32_t out_pa, uint32_t out_cap,
                                  uint32_t self_pid, uint32_t peer_pid,
                                  uint32_t peer_cid);
    uint32_t AnswerClientInit(const Msm8255OncrpcCall& call, uint32_t size,
                              uint32_t out_pa, uint32_t out_cap,
                              uint32_t self_pid, uint32_t peer_pid,
                              uint32_t peer_cid);
    uint32_t AnswerRequestCb(const Msm8255OncrpcCall& call, uint32_t size,
                             uint32_t out_pa, uint32_t out_cap,
                             uint32_t self_pid, uint32_t peer_pid,
                             uint32_t peer_cid);

    uint32_t client_registered_   = 0u;
    uint32_t callback_registered_ = 0u;
    uint32_t callback_id_         = 0u;
};

uint32_t Msm8255TimeRemoteServer::AnswerCall(uint32_t in_pa, uint32_t size,
                                             uint32_t out_pa, uint32_t out_cap,
                                             uint32_t self_pid,
                                             uint32_t peer_pid,
                                             uint32_t peer_cid) {
    const Msm8255OncrpcCall call =
        emu_.Get<Msm8255OncrpcCodec>().ParseCall(*this, in_pa, size);
    switch (call.proc) {
    case kProcSetDefaultTime:
        return AnswerSetDefaultTime(call, size, out_pa, out_cap, self_pid,
                                    peer_pid, peer_cid);
    case kProcClientInit:
        return AnswerClientInit(call, size, out_pa, out_cap, self_pid,
                                peer_pid, peer_cid);
    case kProcRequestCb:
        return AnswerRequestCb(call, size, out_pa, out_cap, self_pid, peer_pid,
                               peer_cid);
    default:
        emu_.Get<Fatal>().Die(
            "msm8255 time remote server: rpc procedure %u with a %u-byte "
            "payload is not modeled", call.proc, size);
    }
}

uint32_t Msm8255TimeRemoteServer::AnswerSetDefaultTime(
    const Msm8255OncrpcCall& call, uint32_t size, uint32_t out_pa,
    uint32_t out_cap, uint32_t self_pid, uint32_t peer_pid,
    uint32_t peer_cid) {
    auto& codec = emu_.Get<Msm8255OncrpcCodec>();
    codec.RequireCallBytes(*this, call.proc, size, kNoArgCallBytes);
    if (callback_registered_ != 0u) {
        emu_.Get<Fatal>().Die(
            "msm8255 time remote server: procedure %u with time callback %u "
            "registered is not modeled", call.proc, callback_id_);
    }
    return codec.WriteAcceptedReply(out_pa, out_cap, self_pid, kTimeCid,
                                    peer_pid, peer_cid, call.xid, nullptr, 0u);
}

uint32_t Msm8255TimeRemoteServer::AnswerClientInit(
    const Msm8255OncrpcCall& call, uint32_t size, uint32_t out_pa,
    uint32_t out_cap, uint32_t self_pid, uint32_t peer_pid,
    uint32_t peer_cid) {
    auto& mem   = emu_.Get<EmulatedMemory>();
    auto& codec = emu_.Get<Msm8255OncrpcCodec>();

    codec.RequireCallBytes(*this, call.proc, size, kInitCallBytes);
    const uint32_t type      = Be32(mem.ReadWord(call.body + kInitTypeOff));
    const uint32_t id_ptr    = Be32(mem.ReadWord(call.body + kInitIdPtrOff));
    const uint32_t id        = Be32(mem.ReadWord(call.body + kInitIdOff));
    const uint32_t processor = Be32(mem.ReadWord(call.body + kInitProcessorOff));

    if (type != kClientTypeInternal || id_ptr != kXdrTrue ||
        id > kByteMax || processor != kProcessorApp1) {
        emu_.Get<Fatal>().Die(
            "msm8255 time remote server: client init of type %u, id pointer "
            "%u, id %u, processor %u is not modeled", type, id_ptr, id,
            processor);
    }
    if (client_registered_ != 0u) {
        emu_.Get<Fatal>().Die(
            "msm8255 time remote server: client init of type %u with client "
            "%u already registered is not modeled", type, kIssuedClientId);
    }

    client_registered_ = 1u;
    const uint32_t results[3] = {kErrNone, kXdrTrue, kIssuedClientId};
    return codec.WriteAcceptedReply(out_pa, out_cap, self_pid, kTimeCid,
                                    peer_pid, peer_cid, call.xid, results, 3u);
}

uint32_t Msm8255TimeRemoteServer::AnswerRequestCb(
    const Msm8255OncrpcCall& call, uint32_t size, uint32_t out_pa,
    uint32_t out_cap, uint32_t self_pid, uint32_t peer_pid,
    uint32_t peer_cid) {
    auto& mem   = emu_.Get<EmulatedMemory>();
    auto& codec = emu_.Get<Msm8255OncrpcCodec>();

    codec.RequireCallBytes(*this, call.proc, size, kCbCallBytes);
    const uint32_t client = Be32(mem.ReadWord(call.body + kCbClientIdOff));
    const uint32_t cb_id  = Be32(mem.ReadWord(call.body + kCbIdOff));

    if (client_registered_ == 0u || client != kIssuedClientId) {
        emu_.Get<Fatal>().Die(
            "msm8255 time remote server: callback request for client %u, "
            "registered %u, is not modeled", client, client_registered_);
    }
    if (callback_registered_ != 0u) {
        emu_.Get<Fatal>().Die(
            "msm8255 time remote server: callback %u requested with callback "
            "%u already registered is not modeled", cb_id, callback_id_);
    }

    callback_registered_ = 1u;
    callback_id_         = cb_id;
    const uint32_t results[1] = {kErrNone};
    return codec.WriteAcceptedReply(out_pa, out_cap, self_pid, kTimeCid,
                                    peer_pid, peer_cid, call.xid, results, 1u);
}

}

REGISTER_SERVICE(Msm8255TimeRemoteServer);

}
