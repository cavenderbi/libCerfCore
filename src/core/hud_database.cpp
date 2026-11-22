#include <cerfcore/core/hud_database.h>

#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/cerf_paths.h>
#include <cerfcore/core/config_json.h>
#include <cerfcore/host/virtual_key_names.h>

#include "name_table.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace cerfcore {

REGISTER_SERVICE(HudDatabase);

namespace {

using nlohmann::json;

constexpr char kHuds[]           = "huds";
constexpr char kElements[]       = "elements";
constexpr char kDevices[]        = "devices";
constexpr char kId[]             = "id";
constexpr char kName[]           = "name";
constexpr char kBoardIds[]       = "board_ids";
constexpr char kBoardFamilyIds[] = "board_family_ids";
constexpr char kPanels[]         = "panels";
constexpr char kSide[]           = "side";
constexpr char kAlign[]          = "align";
constexpr char kWidth[]          = "width";
constexpr char kHeight[]         = "height";
constexpr char kContent[]        = "content";
constexpr char kRow[]            = "row";
constexpr char kColumn[]         = "column";
constexpr char kButton[]         = "button";
constexpr char kDpad[]           = "dpad";
constexpr char kRocker[]         = "rocker";
constexpr char kGrow[]           = "grow";
constexpr char kKeys[]           = "keys";
constexpr char kLabel[]          = "label";
constexpr char kIcon[]           = "icon";
constexpr char kSticker[]        = "sticker";
constexpr char kAction[]         = "action";
constexpr char kHeld[]           = "held";
constexpr char kUp[]             = "up";
constexpr char kDown[]           = "down";
constexpr char kLeft[]           = "left";
constexpr char kRight[]          = "right";
constexpr char kCenter[]         = "center";
constexpr char kShape[]          = "shape";
constexpr char kLamp[]           = "lamp";
constexpr char kDisplay[]        = "display";
constexpr char kObject[]         = "object";
constexpr char kIndicators[]     = "indicators";
constexpr char kX[]              = "x";
constexpr char kY[]              = "y";
constexpr char kSize[]           = "size";
constexpr char kAspect[]         = "aspect";

constexpr std::pair<std::string_view, HudSide> kSides[] = {
    {"top", HudSide::kTop}, {"bottom", HudSide::kBottom}, {"left", HudSide::kLeft}, {"right", HudSide::kRight}};

constexpr std::pair<std::string_view, HudAlign> kAligns[] = {
    {"start", HudAlign::kStart}, {"center", HudAlign::kCenter}, {"end", HudAlign::kEnd}};

constexpr std::pair<std::string_view, HudDpadShape> kShapes[] = {
    {"rounded", HudDpadShape::kRounded}, {"circle", HudDpadShape::kCircle}};

constexpr std::pair<std::string_view, HudLampShape> kLampShapes[] = {{"circle", HudLampShape::kCircle},
                                                                     {"disc", HudLampShape::kDisc}};

std::vector<uint8_t> Keys(const json& list) {
    std::vector<uint8_t> out;
    for (const json& name : list) out.push_back(VirtualKeyByName(name.get<std::string>()).value());
    return out;
}

std::vector<uint8_t> OptKeys(const json& object, const char* key) {
    const auto it = object.find(key);
    return it == object.end() ? std::vector<uint8_t>() : Keys(*it);
}

std::vector<std::string> ObjectPath(const json& object) { return object.at(kObject).get<std::vector<std::string>>(); }

HudLamp Lamp(const json& object) {
    HudLamp lamp;
    lamp.object = ObjectPath(object);
    lamp.shape  = ValueByName(kLampShapes, object.value(kShape, std::string("circle")));
    return lamp;
}

HudIndicator Indicator(const json& object) {
    HudIndicator indicator;
    indicator.node.kind         = HudNode::Kind::kIndicator;
    indicator.node.sticker.lamp = Lamp(object.at(kLamp));
    indicator.x                 = object.at(kX).get<double>();
    indicator.y                 = object.at(kY).get<double>();
    indicator.size              = object.at(kSize).get<double>();
    indicator.aspect            = object.value(kAspect, 1.0);
    return indicator;
}

HudFace Face(const json& object) {
    HudFace face;
    face.label = object.value(kLabel, std::string());
    face.icon  = object.value(kIcon, std::string());
    return face;
}

std::vector<HudLayerFace> LayerFaces(const json& object) {
    std::vector<HudLayerFace> out;
    const auto                held = object.find(kHeld);
    if (held == object.end()) return out;
    for (const auto& [name, face] : held->items()) out.push_back({VirtualKeyByName(name).value(), Face(face)});
    return out;
}

HudButton Button(const json& object) {
    HudButton button;
    button.keys        = OptKeys(object, kKeys);
    button.action      = object.value(kAction, std::vector<std::string>());
    button.face        = Face(object);
    button.layer_faces = LayerFaces(object);
    if (const auto lamp = object.find(kLamp); lamp != object.end()) button.face.lamp = Lamp(*lamp);
    return button;
}

HudDpad Rocker(const json& object) {
    HudDpad rocker;
    rocker.up     = Keys(object.at(kUp));
    rocker.down   = Keys(object.at(kDown));
    rocker.center = OptKeys(object, kCenter);
    return rocker;
}

HudDpad Dpad(const json& object) {
    HudDpad dpad = Rocker(object);
    dpad.left    = Keys(object.at(kLeft));
    dpad.right   = Keys(object.at(kRight));
    dpad.shape   = ValueByName(kShapes, object.value(kShape, std::string("rounded")));
    return dpad;
}

class HudReader {
public:
    explicit HudReader(const json& elements) : elements_(elements) {}

    HudNode Node(const json& value) const {
        if (value.is_string()) return Node(elements_.at(value.get<std::string>()));
        HudNode node;
        node.grow = value.value(kGrow, 1.0);
        if (const auto row = value.find(kRow); row != value.end()) {
            node.kind     = HudNode::Kind::kRow;
            node.children = Children(*row);
        } else if (const auto column = value.find(kColumn); column != value.end()) {
            node.kind     = HudNode::Kind::kColumn;
            node.children = Children(*column);
        } else if (const auto button = value.find(kButton); button != value.end()) {
            node.kind   = HudNode::Kind::kButton;
            node.button = Button(*button);
        } else if (const auto dpad = value.find(kDpad); dpad != value.end()) {
            node.kind = HudNode::Kind::kDpad;
            node.dpad = Dpad(*dpad);
        } else if (const auto rocker = value.find(kRocker); rocker != value.end()) {
            node.kind = HudNode::Kind::kDpad;
            node.dpad = Rocker(*rocker);
        } else if (const auto sticker = value.find(kSticker); sticker != value.end()) {
            node.kind    = HudNode::Kind::kSticker;
            node.sticker = Face(*sticker);
        } else if (const auto lamp = value.find(kLamp); lamp != value.end()) {
            node.kind         = HudNode::Kind::kSticker;
            node.sticker.lamp = Lamp(*lamp);
        } else if (const auto display = value.find(kDisplay); display != value.end()) {
            node.kind            = HudNode::Kind::kSticker;
            node.sticker.display = ObjectPath(*display);
        } else {
            node.kind = HudNode::Kind::kSpace;
        }
        return node;
    }

    HudPanel Panel(const json& object) const {
        HudPanel panel;
        panel.side   = ValueByName(kSides, object.at(kSide).get<std::string>());
        panel.align  = ValueByName(kAligns, object.value(kAlign, std::string("center")));
        panel.width  = object.at(kWidth).get<double>();
        panel.height = object.at(kHeight).get<double>();
        panel.root   = Node(object.at(kContent));
        if (const auto list = object.find(kIndicators); list != object.end())
            for (const json& item : *list) panel.indicators.push_back(Indicator(item));
        return panel;
    }

private:
    std::vector<HudNode> Children(const json& list) const {
        std::vector<HudNode> out;
        for (const json& child : list) out.push_back(Node(child));
        return out;
    }

    const json& elements_;
};

bool Contains(const std::vector<std::string>& list, std::string_view id) {
    return std::find(list.begin(), list.end(), id) != list.end();
}

}

void HudDatabase::OnReady() {
    const json      root = CfgReadInstalledJsonFile(DbJsonPath());
    const json&     huds = root.at(kHuds);
    const HudReader reader(huds.at(kElements));
    for (const json& h : huds.at(kDevices)) {
        DbHud hud;
        hud.id               = h.at(kId).get<std::string>();
        hud.name             = h.at(kName).get<std::string>();
        hud.board_ids        = h.value(kBoardIds, std::vector<std::string>());
        hud.board_family_ids = h.value(kBoardFamilyIds, std::vector<std::string>());
        for (const DbBoard& board : emu_.Get<BoardDatabase>().Boards()) {
            if (!board.lcd_panel_size) continue;
            if (!Contains(hud.board_ids, board.id) && !Contains(hud.board_family_ids, board.family_id)) continue;
            hud.screens.push_back(*board.lcd_panel_size);
        }
        for (const json& panel : h.at(kPanels)) hud.panels.push_back(reader.Panel(panel));
        huds_.push_back(std::move(hud));
    }
}

const DbLcdPanelSize& DbHud::ScreenFor(double width, double height) const {
    const DbLcdPanelSize* best = &screens.at(0);
    double                best_distance = 0.0;
    for (const DbLcdPanelSize& screen : screens) {
        const double distance = std::abs(screen.width - width) + std::abs(screen.height - height);
        if (&screen != best && distance >= best_distance) continue;
        best          = &screen;
        best_distance = distance;
    }
    return *best;
}

const DbHud* HudDatabase::Find(std::string_view id) const {
    for (const DbHud& hud : huds_)
        if (hud.id == id) return &hud;
    return nullptr;
}

const DbHud* HudDatabase::AutoFor(std::string_view board_id, std::string_view board_family_id) const {
    for (const DbHud& hud : huds_)
        if (Contains(hud.board_ids, board_id)) return &hud;
    for (const DbHud& hud : huds_)
        if (Contains(hud.board_family_ids, board_family_id)) return &hud;
    return nullptr;
}

}
