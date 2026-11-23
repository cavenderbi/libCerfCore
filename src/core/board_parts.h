#pragma once

#include <cerfcore/core/board_database.h>
#include <cerfcore/core/service.h>

#include <optional>
#include <string_view>
#include <vector>

namespace cerfcore {

class BoardParts : public Service {
public:
    using Service::Service;

    void OnReady() override;

    const DbPart* Find(std::string_view generic_part_id) const;
    bool          Has(std::string_view part_id) const;

    std::optional<DbLcdPanelSize> ViewportClamp() const;

private:
    std::vector<const DbPart*> parts_;
};

}
