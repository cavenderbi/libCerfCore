#include "msm8255_oncrpc_codec.h"
#include "msm8255_rpc_server.h"
#include "msm8255_rpc_server_registry.h"
#include "msm8255_rpcrouter_wire.h"

#include <cerfcore/boards/board_context.h>
#include "msm8255_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/log.h>
#include <cerfcore/cpu/emulated_memory.h>

#include <cstdint>
#include <cstdio>
#include <string>

namespace cerfcore {

namespace {

constexpr uint32_t kCmProg = 0x30000000u;
constexpr uint32_t kCmVers = 0x002D0001u;
constexpr uint32_t kCmCid  = 7u;

class Msm8255CmRemoteServer : public Msm8255RpcServer {
public:
    using Msm8255RpcServer::Msm8255RpcServer;

    bool ShouldRegister() override {
        auto* bd = emu_.TryGet<BoardContext>();
        return bd && bd->GetSocId() == SocId::Msm8255;
    }

    void OnReady() override {
        emu_.Get<Msm8255RpcServerRegistry>().Register(this);
    }

    uint32_t ServerProg() const override { return kCmProg; }
    uint32_t ServerVers() const override { return kCmVers; }
    uint32_t ServerCid() const override { return kCmCid; }

    uint32_t AnswerCall(uint32_t in_pa, uint32_t size, uint32_t out_pa,
                        uint32_t out_cap, uint32_t self_pid, uint32_t peer_pid,
                        uint32_t peer_cid) override {
        auto& codec = emu_.Get<Msm8255OncrpcCodec>();
        const Msm8255OncrpcCall call = codec.ParseCall(*this, in_pa, size);
        auto& mem = emu_.Get<EmulatedMemory>();
        const uint32_t arg_bytes = size - (kPacmarkBytes + kCallArgsOff);
        std::string args;
        for (uint32_t off = 0; off + 4u <= arg_bytes; off += 4u) {
            char word[12];
            std::snprintf(word, sizeof(word), " %08X", Be32(mem.ReadWord(call.body + kCallArgsOff + off)));
            args += word;
        }
        LOG(SocModem, "msm8255 cm remote server: procedure %u, arguments%s, answered PROC_UNAVAIL\n",
            call.proc, args.c_str());
        return codec.WriteProcUnavailReply(out_pa, out_cap, self_pid, kCmCid,
                                           peer_pid, peer_cid, call.xid);
    }
};

}

REGISTER_SERVICE(Msm8255CmRemoteServer);

}
