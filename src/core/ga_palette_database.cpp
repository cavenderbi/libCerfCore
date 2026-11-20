#include <cerfcore/core/ga_palette_database.h>

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/cerf_paths.h>
#include <cerfcore/core/config_json.h>
#include <cerfcore/core/ga_palette_json.h>

#include <nlohmann/json.hpp>

namespace cerfcore {

REGISTER_SERVICE(GaPaletteDatabase);

namespace {

constexpr char kGaPalettes[] = "ga_palettes";
constexpr char kId[]         = "id";
constexpr char kName[]       = "name";
constexpr char kBpp[]        = "bpp";
constexpr char kValues[]     = "values";

}

void GaPaletteDatabase::OnReady() {
    const nlohmann::json root = CfgReadInstalledJsonFile(PalettesJsonPath());
    for (const auto& p : root.at(kGaPalettes))
        palettes_.push_back(DbGaPalette{p.at(kId).get<std::string>(), p.at(kName).get<std::string>(),
                                        p.at(kBpp).get<uint32_t>(), GaPaletteFromJson(p.at(kValues)).value()});
}

const DbGaPalette* GaPaletteDatabase::DefaultFor(uint32_t bpp) const {
    for (const auto& p : palettes_)
        if (p.bpp == bpp) return &p;
    return nullptr;
}

}
