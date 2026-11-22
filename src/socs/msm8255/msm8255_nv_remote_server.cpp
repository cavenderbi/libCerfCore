#include "msm8255_modem_nv_store.h"
#include "msm8255_oncrpc_codec.h"
#include "msm8255_rpc_server.h"
#include "msm8255_rpc_server_registry.h"
#include "msm8255_rpcrouter_wire.h"

#include <cerfcore/boards/board_context.h>
#include "msm8255_id.h"
#include <cerfcore/core/byte_order.h>
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/cpu/emulated_memory.h>

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kNvProg = 0x3000000Eu;
constexpr uint32_t kNvVers = 0x00090001u;
constexpr uint32_t kNvCid  = 6u;

constexpr uint32_t kProcPrlVersion  = 7u;
constexpr uint32_t kProcNvCmdRemote = 9u;

constexpr uint32_t kNvCmdRead  = 0u;
constexpr uint32_t kNvCmdWrite = 1u;

constexpr uint32_t kNvStatusOk       = 0u;
constexpr uint32_t kNvStatusInactive = 5u;

constexpr uint32_t kPrlVersionNone = 0xFFFFu;
constexpr uint32_t kByteMax        = 0xFFu;

constexpr uint32_t kArgCmdOff      = kCallArgsOff + 0u;
constexpr uint32_t kArgItemOff     = kCallArgsOff + 4u;
constexpr uint32_t kArgDataPtrOff  = kCallArgsOff + 8u;
constexpr uint32_t kArgUnionTagOff = kCallArgsOff + 12u;
constexpr uint32_t kArgUnionOff    = kCallArgsOff + 16u;

constexpr uint32_t kPrlNamOff    = kCallArgsOff;
constexpr uint32_t kPrlCallBytes = kPacmarkBytes + kCallArgsOff + 4u;

enum class NvUnion : uint8_t { kByte, kTwoBytes, kWords31 };

struct NvItemLayout {
    uint32_t item;
    NvUnion  form;
};

constexpr NvItemLayout kItemLayouts[] = {
    {74u, NvUnion::kTwoBytes},  {179u, NvUnion::kByte},     {1302u, NvUnion::kByte},
    {3461u, NvUnion::kByte},    {5153u, NvUnion::kByte},    {5895u, NvUnion::kByte},
    {6857u, NvUnion::kWords31}, {6859u, NvUnion::kWords31}, {6860u, NvUnion::kWords31},
};

constexpr uint32_t kWords31Count   = 31u;
constexpr uint32_t kResultHeadWords = 3u;
constexpr uint32_t kResultMaxWords  = kResultHeadWords + kWords31Count;

uint32_t UnionWords(NvUnion form) {
    switch (form) {
    case NvUnion::kByte:     return 1u;
    case NvUnion::kTwoBytes: return 2u;
    case NvUnion::kWords31:  return kWords31Count;
    }
    return 0u;
}

const NvItemLayout* FindLayout(uint32_t item) {
    for (const NvItemLayout& layout : kItemLayouts) {
        if (layout.item == item) return &layout;
    }
    return nullptr;
}

class Msm8255NvRemoteServer : public Msm8255RpcServer {
public:
    using Msm8255RpcServer::Msm8255RpcServer;

    bool ShouldRegister() override {
        auto* bd = emu_.TryGet<BoardContext>();
        return bd && bd->GetSocId() == SocId::Msm8255;
    }

    void OnReady() override {
        emu_.Get<Msm8255RpcServerRegistry>().Register(this);
    }

    uint32_t ServerProg() const override { return kNvProg; }
    uint32_t ServerVers() const override { return kNvVers; }
    uint32_t ServerCid() const override { return kNvCid; }

    uint32_t AnswerCall(uint32_t in_pa, uint32_t size, uint32_t out_pa,
                        uint32_t out_cap, uint32_t self_pid, uint32_t peer_pid,
                        uint32_t peer_cid) override;

private:
    struct Reply {
        uint32_t out_pa;
        uint32_t out_cap;
        uint32_t self_pid;
        uint32_t peer_pid;
        uint32_t peer_cid;
        uint32_t xid;
    };

    uint32_t AnswerPrlVersion(const Msm8255OncrpcCall& call, uint32_t size, const Reply& reply);
    uint32_t AnswerNvCmd(const Msm8255OncrpcCall& call, uint32_t size, const Reply& reply);
    uint32_t AnswerRead(uint32_t item, const NvItemLayout* layout, const Reply& reply);
    uint32_t AnswerWrite(const Msm8255OncrpcCall& call, uint32_t item, const NvItemLayout* layout,
                         uint32_t data_ptr, const Reply& reply);
    uint32_t Send(const Reply& reply, const uint32_t* results, uint32_t words);
};

uint32_t Msm8255NvRemoteServer::AnswerCall(uint32_t in_pa, uint32_t size,
                                           uint32_t out_pa, uint32_t out_cap,
                                           uint32_t self_pid, uint32_t peer_pid,
                                           uint32_t peer_cid) {
    const Msm8255OncrpcCall call = emu_.Get<Msm8255OncrpcCodec>().ParseCall(*this, in_pa, size);
    const Reply reply{out_pa, out_cap, self_pid, peer_pid, peer_cid, call.xid};
    switch (call.proc) {
    case kProcPrlVersion:
        return AnswerPrlVersion(call, size, reply);
    case kProcNvCmdRemote:
        return AnswerNvCmd(call, size, reply);
    default:
        emu_.Get<Fatal>().Die(
            "msm8255 nv remote server: rpc procedure %u with a %u-byte payload "
            "is not modeled", call.proc, size);
    }
}

uint32_t Msm8255NvRemoteServer::Send(const Reply& reply, const uint32_t* results, uint32_t words) {
    return emu_.Get<Msm8255OncrpcCodec>().WriteAcceptedReply(
        reply.out_pa, reply.out_cap, reply.self_pid, kNvCid, reply.peer_pid,
        reply.peer_cid, reply.xid, results, words);
}

uint32_t Msm8255NvRemoteServer::AnswerPrlVersion(const Msm8255OncrpcCall& call, uint32_t size,
                                                 const Reply& reply) {
    emu_.Get<Msm8255OncrpcCodec>().RequireCallBytes(*this, call.proc, size, kPrlCallBytes);
    const uint32_t nam = Be32(emu_.Get<EmulatedMemory>().ReadWord(call.body + kPrlNamOff));
    if (nam > kByteMax) {
        emu_.Get<Fatal>().Die("msm8255 nv remote server: prl version of nam word %u", nam);
    }
    const uint32_t results[1] = {kPrlVersionNone};
    return Send(reply, results, 1u);
}

uint32_t Msm8255NvRemoteServer::AnswerNvCmd(const Msm8255OncrpcCall& call, uint32_t size,
                                            const Reply& reply) {
    auto& mem   = emu_.Get<EmulatedMemory>();
    auto& codec = emu_.Get<Msm8255OncrpcCodec>();

    if (size < kPacmarkBytes + kArgUnionTagOff) {
        emu_.Get<Fatal>().Die(
            "msm8255 nv remote server: procedure %u carries %u payload bytes, "
            "short of its %u-byte fixed arguments", call.proc, size,
            kPacmarkBytes + kArgUnionTagOff);
    }
    const uint32_t cmd      = Be32(mem.ReadWord(call.body + kArgCmdOff));
    const uint32_t item     = Be32(mem.ReadWord(call.body + kArgItemOff));
    const uint32_t data_ptr = Be32(mem.ReadWord(call.body + kArgDataPtrOff));
    const NvItemLayout* layout = FindLayout(item);

    if (data_ptr == kXdrFalse) {
        codec.RequireCallBytes(*this, call.proc, size, kPacmarkBytes + kArgUnionTagOff);
    } else if (data_ptr == kXdrTrue) {
        if (size < kPacmarkBytes + kArgUnionOff) {
            emu_.Get<Fatal>().Die(
                "msm8255 nv remote server: nv command %u on item %u carries %u "
                "payload bytes and no union tag", cmd, item, size);
        }
        const uint32_t tag         = Be32(mem.ReadWord(call.body + kArgUnionTagOff));
        const uint32_t union_bytes = size - (kPacmarkBytes + kArgUnionOff);
        if (tag != item || union_bytes % 4u != 0u ||
            (layout && union_bytes != 4u * UnionWords(layout->form))) {
            emu_.Get<Fatal>().Die(
                "msm8255 nv remote server: nv command %u on item %u carries union "
                "tag %u and %u union bytes", cmd, item, tag, union_bytes);
        }
    } else {
        emu_.Get<Fatal>().Die(
            "msm8255 nv remote server: nv command %u on item %u carries data "
            "pointer flag %u", cmd, item, data_ptr);
    }

    switch (cmd) {
    case kNvCmdRead:
        return AnswerRead(item, layout, reply);
    case kNvCmdWrite:
        return AnswerWrite(call, item, layout, data_ptr, reply);
    default:
        emu_.Get<Fatal>().Die(
            "msm8255 nv remote server: nv command %u on item %u is not modeled", cmd, item);
    }
}

uint32_t Msm8255NvRemoteServer::AnswerRead(uint32_t item, const NvItemLayout* layout,
                                           const Reply& reply) {
    uint8_t data[kModemNvItemDataBytes] = {};
    auto& store = emu_.Get<Msm8255ModemNvStore>();
    if (!store.Find(item, data)) {
        if (store.HoldsAnyIndex(item)) {
            emu_.Get<Fatal>().Die(
                "msm8255 nv remote server: read of nv item %u, which the modem nv "
                "store holds only at another index, is not modeled", item);
        }
        const uint32_t results[2] = {kNvStatusInactive, kXdrFalse};
        return Send(reply, results, 2u);
    }
    if (!layout) {
        emu_.Get<Fatal>().Die(
            "msm8255 nv remote server: read of nv item %u, which the modem nv "
            "store holds, is not modeled", item);
    }
    uint32_t results[kResultMaxWords] = {kNvStatusOk, kXdrTrue, item};
    const uint32_t words = UnionWords(layout->form);
    for (uint32_t i = 0; i < words; ++i) {
        results[kResultHeadWords + i] =
            layout->form == NvUnion::kWords31 ? cerf::le::U32(data, 4u * i) : data[i];
    }
    return Send(reply, results, kResultHeadWords + words);
}

uint32_t Msm8255NvRemoteServer::AnswerWrite(const Msm8255OncrpcCall& call, uint32_t item,
                                            const NvItemLayout* layout, uint32_t data_ptr,
                                            const Reply& reply) {
    if (!layout || data_ptr != kXdrTrue) {
        emu_.Get<Fatal>().Die(
            "msm8255 nv remote server: write of nv item %u with data pointer flag "
            "%u is not modeled", item, data_ptr);
    }
    auto& mem = emu_.Get<EmulatedMemory>();
    uint8_t  data[kModemNvItemDataBytes] = {};
    uint32_t results[kResultMaxWords]    = {kNvStatusOk, kXdrTrue, item};
    const uint32_t words = UnionWords(layout->form);
    for (uint32_t i = 0; i < words; ++i) {
        const uint32_t word = Be32(mem.ReadWord(call.body + kArgUnionOff + 4u * i));
        if (layout->form == NvUnion::kWords31) {
            cerf::le::Put32(data + 4u * i, word);
        } else {
            if (word > kByteMax) {
                emu_.Get<Fatal>().Die(
                    "msm8255 nv remote server: write of nv item %u carries byte word 0x%08X",
                    item, word);
            }
            data[i] = static_cast<uint8_t>(word);
        }
        results[kResultHeadWords + i] = word;
    }
    emu_.Get<Msm8255ModemNvStore>().Store(item, data);
    return Send(reply, results, kResultHeadWords + words);
}

}

REGISTER_SERVICE(Msm8255NvRemoteServer);

}
