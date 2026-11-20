#pragma once

#include <cerfcore/core/service.h>
#include <cerfcore/host/machine_socket.h>
#include "../pcmcia/pcmcia_card_catalog.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cerfcore {

class CompactFlashCommands : public Service {
public:
    using Service::Service;

    std::vector<InsertCommand> Commands() const;
    void Run(MachineObject& socket, const std::string& command_id, const std::string& argument,
             PcmciaCardCatalog::CardInserter inserter);

private:
    enum class Fat : uint8_t { k16, k32 };

    std::string CfImgPath() const;
    std::string BundledPath(const std::string& file) const;
    uint64_t    PayloadMb(const std::vector<std::string>& files) const;
    void        Generate(MachineObject& socket, Fat fat, PcmciaCardCatalog::CardInserter inserter);
    void        AskOutput(MachineObject& socket, Fat fat, const std::vector<std::string>& files, uint64_t size_mb,
                          const PcmciaCardCatalog::CardInserter& inserter);
    void        Build(Fat fat, const std::vector<std::string>& files, uint64_t size_mb, const std::string& cf,
                      const PcmciaCardCatalog::CardInserter& inserter);
};

}
