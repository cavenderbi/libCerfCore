#include "compactflash_commands.h"

#include "compactflash_card.h"
#include "compactflash_fat16.h"
#include "compactflash_fat32.h"

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/cerf_paths.h>
#include <cerfcore/core/device_config.h>
#include <cerfcore/core/log.h>
#include <cerfcore/host/machine_requests.h>
#include <cerfcore/platform/host_file.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

namespace cerfcore {

namespace {

constexpr char     kInsertBundled[]      = "insert_bundled";
constexpr char     kChooseImage[]        = "choose_image";
constexpr char     kChooseDefaultImage[] = "choose_default_image";
constexpr char     kGenerateFat16[]      = "generate_fat16";
constexpr char     kGenerateFat32[]      = "generate_fat32";
constexpr uint64_t kFat16MinimumMb       = 4;
constexpr uint64_t kFat32MinimumMb       = 32;

}

REGISTER_SERVICE(CompactFlashCommands);

std::string CompactFlashCommands::CfImgPath() const { return GetCerfDir() + kDefaultCfImageName; }

std::string CompactFlashCommands::BundledPath(const std::string& file) const {
    return ResolveDeviceFile(emu_.Get<DeviceConfig>().device_name, file);
}

uint64_t CompactFlashCommands::PayloadMb(const std::vector<std::string>& files) const {
    uint64_t total = 0;
    for (const auto& f : files) {
        uint64_t size = 0;
        if (host_platform::HostRegularFileSize(f, size)) total += size;
    }
    return (total + (1u << 20) - 1) >> 20;
}

std::vector<InsertCommand> CompactFlashCommands::Commands() const {
    std::vector<InsertCommand> commands;
    InsertCommand bundled{kInsertBundled, {}};
    for (const auto& card : emu_.Get<DeviceConfig>().bundled_compact_flash_cards)
        if (host_platform::HostPathExists(BundledPath(card.file)))
            bundled.arguments.push_back(InsertArgument{card.file, card.name});
    if (!bundled.arguments.empty()) commands.push_back(std::move(bundled));
    commands.push_back(InsertCommand{kChooseImage, {}});
    if (host_platform::HostPathExists(CfImgPath())) commands.push_back(InsertCommand{kChooseDefaultImage, {}});
    commands.push_back(InsertCommand{kGenerateFat16, {}});
    commands.push_back(InsertCommand{kGenerateFat32, {}});
    return commands;
}

void CompactFlashCommands::Run(MachineObject& socket, const std::string& command_id, const std::string& argument,
                               PcmciaCardCatalog::CardInserter inserter) {
    if (command_id == kInsertBundled) {
        inserter.insert(std::make_unique<CompactFlashCard>(emu_, BundledPath(argument)));
        return;
    }
    if (command_id == kChooseImage) {
        MachineRequest request;
        request.kind       = MachineRequestKind::kPath;
        request.id         = "compact_flash_image";
        request.object     = &socket;
        request.extensions = {"img", "bin", "ima"};
        emu_.Get<MachineRequests>().Ask(std::move(request), [this, inserter](const MachineReply& reply) {
            if (reply.cancelled) return;
            inserter.insert(std::make_unique<CompactFlashCard>(emu_, reply.path));
        });
        return;
    }
    if (command_id == kChooseDefaultImage) {
        inserter.insert(std::make_unique<CompactFlashCard>(emu_, CfImgPath()));
        return;
    }
    Generate(socket, command_id == kGenerateFat16 ? Fat::k16 : Fat::k32, std::move(inserter));
}

void CompactFlashCommands::Generate(MachineObject& socket, Fat fat, PcmciaCardCatalog::CardInserter inserter) {
    MachineRequest request;
    request.kind   = MachineRequestKind::kDocuments;
    request.id     = "compact_flash_documents";
    request.object = &socket;
    emu_.Get<MachineRequests>().Ask(std::move(request), [this, &socket, fat, inserter](const MachineReply& files) {
        if (files.cancelled || files.documents.empty()) return;
        const uint64_t floor   = fat == Fat::k16 ? kFat16MinimumMb : kFat32MinimumMb;
        const uint64_t minimum = std::max(PayloadMb(files.documents), floor);
        MachineRequest size;
        size.kind        = MachineRequestKind::kSize;
        size.id          = "compact_flash_size_mb";
        size.object      = &socket;
        size.minimum     = minimum;
        size.maximum     = std::numeric_limits<uint32_t>::max();
        size.recommended = minimum;
        emu_.Get<MachineRequests>().Ask(
            std::move(size), [this, &socket, fat, documents = files.documents, inserter](const MachineReply& r) {
                if (r.cancelled) return;
                AskOutput(socket, fat, documents, r.size, inserter);
            });
    });
}

void CompactFlashCommands::AskOutput(MachineObject& socket, Fat fat, const std::vector<std::string>& files,
                                     uint64_t size_mb, const PcmciaCardCatalog::CardInserter& inserter) {
    MachineRequest output;
    output.kind             = MachineRequestKind::kOutputPath;
    output.id               = "compact_flash_output_image";
    output.object           = &socket;
    output.extensions       = {"img"};
    output.recommended_path = CfImgPath();
    emu_.Get<MachineRequests>().Ask(std::move(output),
                                    [this, fat, files, size_mb, inserter](const MachineReply& r) {
                                        if (r.cancelled) return;
                                        Build(fat, files, size_mb, r.path, inserter);
                                    });
}

void CompactFlashCommands::Build(Fat fat, const std::vector<std::string>& files, uint64_t size_mb,
                                 const std::string& cf, const PcmciaCardCatalog::CardInserter& inserter) {
    const uint32_t    mb    = static_cast<uint32_t>(size_mb);
    const bool        built = fat == Fat::k16 ? emu_.Get<CompactFlashFat16Builder>().Build(cf, files, mb)
                                              : emu_.Get<CompactFlashFat32Builder>().Build(cf, files, mb);
    if (!built) {
        LOG(Pcmcia, "[CF] the %s image build into %s failed\n", fat == Fat::k16 ? "FAT16" : "FAT32", cf.c_str());
        inserter.fail("image_build_failed", cf);
        return;
    }
    inserter.insert(std::make_unique<CompactFlashCard>(emu_, cf));
}

}
