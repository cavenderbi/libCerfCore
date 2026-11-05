#include <cerfcore/core/service.h>
#include <cerfcore/host/host_widget.h>
#include <cstdint>
#include <string>
#include <vector>
#include "ford_sync_gen2_temperature_section.h"
#include "ford_sync_gen2_media_status.h"
#include "ford_sync_gen2_entertainment.h"
#include "ford_sync_gen2_console.h"
#include <cerfcore/boards/board_context.h>
#include "ford_sync_gen2_family_id.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/host/host_widget_registry.h>
#include <cerfcore/state/state_stream.h>

namespace cerfcore {

namespace {

class FordSyncGen2CarWidget : public Service, public HostWidget {
public:
    explicit FordSyncGen2CarWidget(CerfEmulator& emu)
        : Service(emu), temperature_(emu), sections_{&temperature_} {}
    bool ShouldRegister() override;
    void OnReady() override;
    std::string WidgetName() const override { return "Car"; }
    WidgetGroup Group() const override { return WidgetGroup::Indicator; }
    std::string Tooltip() const override;
    std::vector<WidgetControl> Controls() override;
    WidgetIcon Icon() const override { return ResourceIcon{"car"}; }
    bool PollDirty() override;
    void SaveWidgetState(StateWriter& w) const override;
    void RestoreWidgetState(StateReader& r) override;
private:
    TemperatureSection temperature_;
    std::vector<WidgetSection*> sections_;
    std::string last_entertainment_;
    std::string EntertainmentText() const;
};

REGISTER_SERVICE(FordSyncGen2CarWidget);

bool FordSyncGen2CarWidget::ShouldRegister() {
    auto* board = emu_.TryGet<BoardContext>();
    return board && board->GetBoardFamilyId() == BoardFamilyId::FordSyncGen2;
}
void FordSyncGen2CarWidget::OnReady() { emu_.Get<HostWidgetRegistry>().Register(this); }
std::string FordSyncGen2CarWidget::Tooltip() const {
    return "SYNC 2 car controls\n" + temperature_.Label() + "\n" + EntertainmentText();
}
std::vector<WidgetControl> FordSyncGen2CarWidget::Controls() {
    std::vector<WidgetControl> controls;
    for (auto* section : sections_)
        controls.push_back(WidgetControl{GroupControl{section->Label(), section->BuildControls()}});
    std::vector<WidgetControl> entertainment{WidgetControl{InfoControl{EntertainmentText()}}};
    for (auto& action : emu_.Get<FordSyncGen2Console>().Actions())
        entertainment.push_back(WidgetControl{std::move(action)});
    controls.push_back(WidgetControl{GroupControl{"Entertainment", std::move(entertainment)}});
    return controls;
}
bool FordSyncGen2CarWidget::PollDirty() {
    bool dirty = false;
    for (auto* section : sections_) dirty |= section->PollDirty();
    const auto current = EntertainmentText();
    dirty |= current != last_entertainment_;
    last_entertainment_ = current;
    return dirty;
}
std::string FordSyncGen2CarWidget::EntertainmentText() const {
    const auto owner = emu_.Get<FordSyncGen2Entertainment>().Snapshot();
    const std::string source = owner.Active() ? "Entertainment source active" : "No entertainment source";
    const auto& console = emu_.Get<FordSyncGen2Console>();
    return source + " · USB playback: " + emu_.Get<FordSyncGen2MediaStatus>().ReadProgress().Text() +
        " · Output " + (console.Muted() ? std::string("muted") : std::to_string(console.Volume()) + "/30");
}
void FordSyncGen2CarWidget::SaveWidgetState(StateWriter& w) const { temperature_.SaveState(w); }
void FordSyncGen2CarWidget::RestoreWidgetState(StateReader& r) { temperature_.RestoreState(r); }
}

}
