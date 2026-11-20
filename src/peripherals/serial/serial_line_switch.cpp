#include "serial_line_switch.h"

#include <utility>

namespace cerfcore {

SerialLineSwitch::SerialLineSwitch(CerfEmulator& emu, uint32_t lines)
    : emu_(emu),
      lines_(lines, nullptr),
      timer_(emu, [this] { port_.OnTimer(); }),
      port_(*this, timer_, [this] { return ControlLines(); }) {
    taps_.reserve(lines);
    for (uint32_t i = 0; i < lines; ++i) taps_.push_back(std::make_unique<Tap>(*this, i));
}

void SerialLineSwitch::Attach(ObjectIdentity identity) {
    timer_.AttachLineTimer();
    port_.AttachCradle(emu_, std::move(identity));
    port_.SetActivityObject(nullptr);
}

MachineObject* SerialLineSwitch::Activity() const { return port_.CradleObject(); }

void SerialLineSwitch::AttachLine(uint32_t index, SerialLine& line) {
    std::lock_guard<std::recursive_mutex> lk(route_mu_);
    lines_[index] = &line;
    if (selected_ == index) {
        selected_line_.store(&line, std::memory_order_release);
        ConnectLocked(line, index);
    } else if (joined_ == index && plugged_) {
        line.SetEndpoint(taps_[index].get());
    }
}

void SerialLineSwitch::Select(std::optional<uint32_t> index, std::optional<uint32_t> joined) {
    if (joined == index) joined.reset();
    {
        std::lock_guard<std::recursive_mutex> lk(route_mu_);
        if (index == selected_ && joined == joined_) return;
        if (SerialLine* before = LineAt(selected_)) DisconnectLocked(*before, *selected_);
        if (SerialLine* before = LineAt(joined_)) DisconnectLocked(*before, *joined_);
        selected_         = index;
        joined_           = joined;
        SerialLine* after = LineAt(index);
        selected_line_.store(after, std::memory_order_release);
        selected_index_.store(index ? *index : kNoneLine, std::memory_order_release);
        if (after != nullptr) ConnectLocked(*after, *index);
        SerialLine* joined_line = LineAt(joined);
        if (joined_line != nullptr && plugged_) joined_line->SetEndpoint(taps_[*joined].get());
    }
    port_.NotifyControlLines();
}

void SerialLineSwitch::AddInputsListener(CallbackList<bool, bool, bool, bool>::Callback listener) {
    inputs_listeners_.Add(std::move(listener));
}

void SerialLineSwitch::OnShutdown() { port_.OnShutdown(); }

void SerialLineSwitch::SaveSwitchState(StateWriter& w) {
    port_.SaveCradleState(w);
    timer_.SaveLineTimer(w, "endpoint_timer");
    port_.SaveEndpointState(w);
}

void SerialLineSwitch::RestoreSwitchState(StateReader& r) {
    port_.RestoreCradleState(r);
    timer_.RestoreLineTimer(r, "endpoint_timer");
    port_.RestoreEndpointState(r);
}

void SerialLineSwitch::PostRestore() {
    timer_.ResumeLineTimer();
    port_.PostRestore();
}

void SerialLineSwitch::PushRx(const uint8_t* data, size_t n) {
    SerialLine* line = selected_line_.load(std::memory_order_acquire);
    if (line != nullptr) line->PushRx(data, n);
}

bool SerialLineSwitch::RxEmpty() const {
    const SerialLine* line = selected_line_.load(std::memory_order_acquire);
    return line == nullptr || line->RxEmpty();
}

void SerialLineSwitch::SetRxDrainCallback(RxDrainFn cb) {
    std::lock_guard<std::recursive_mutex> lk(route_mu_);
    drain_ = std::move(cb);
    if (SerialLine* line = LineAt(selected_)) line->SetRxDrainCallback(drain_);
}

void SerialLineSwitch::SetModemInputs(bool cts, bool dsr, bool ri, bool dcd) {
    const uint8_t inputs = static_cast<uint8_t>((cts ? kCts : 0u) | (dsr ? kDsr : 0u) | (ri ? kRi : 0u) |
                                                (dcd ? kDcd : 0u));
    inputs_.store(inputs, std::memory_order_release);
    inputs_listeners_.Call(cts, dsr, ri, dcd);
    SerialLine* line = selected_line_.load(std::memory_order_acquire);
    if (line != nullptr) line->SetModemInputs(cts, dsr, ri, dcd);
}

SerialLine::LineConfig SerialLineSwitch::GetLineConfig() const {
    const SerialLine* line = selected_line_.load(std::memory_order_acquire);
    return line != nullptr ? line->GetLineConfig() : LineConfig{};
}

void SerialLineSwitch::SetLineConfigCallback(LineConfigFn cb) {
    std::lock_guard<std::recursive_mutex> lk(route_mu_);
    line_config_ = std::move(cb);
    if (SerialLine* line = LineAt(selected_)) line->SetLineConfigCallback(line_config_);
}

void SerialLineSwitch::SetEndpoint(SerialEndpoint* ep) {
    std::lock_guard<std::recursive_mutex> lk(route_mu_);
    SerialLine* line        = LineAt(selected_);
    SerialLine* joined_line = LineAt(joined_);
    if (ep == nullptr) {
        if (line != nullptr) line->SetEndpoint(nullptr);
        if (joined_line != nullptr) joined_line->SetEndpoint(nullptr);
        plugged_ = false;
        port_.SetEndpoint(nullptr);
        return;
    }
    port_.SetEndpoint(ep);
    plugged_ = true;
    if (line != nullptr) line->SetEndpoint(taps_[*selected_].get());
    if (joined_line != nullptr) joined_line->SetEndpoint(taps_[*joined_].get());
}

void SerialLineSwitch::ArmEndpointTimer(int64_t delay_ns) { port_.ArmTimer(delay_ns); }

void SerialLineSwitch::DisarmEndpointTimer() { port_.DisarmTimer(); }

SerialControlLines SerialLineSwitch::ControlLines() const {
    SerialControlLines lines;
    lines.dtr = dtr_.load(std::memory_order_acquire);
    lines.rts = rts_.load(std::memory_order_acquire);
    for (const std::unique_ptr<Tap>& tap : taps_) lines.in_break = lines.in_break || tap->InBreak();
    return lines;
}

SerialLine* SerialLineSwitch::LineAt(std::optional<uint32_t> index) const {
    return index ? lines_[*index] : nullptr;
}

void SerialLineSwitch::ConnectLocked(SerialLine& line, uint32_t index) {
    const uint8_t inputs = inputs_.load(std::memory_order_acquire);
    line.SetRxDrainCallback(drain_);
    line.SetLineConfigCallback(line_config_);
    line.SetModemInputs((inputs & kCts) != 0u, (inputs & kDsr) != 0u, (inputs & kRi) != 0u, (inputs & kDcd) != 0u);
    if (plugged_) line.SetEndpoint(taps_[index].get());
}

void SerialLineSwitch::DisconnectLocked(SerialLine& line, uint32_t index) {
    line.SetEndpoint(nullptr);
    line.SetRxDrainCallback(nullptr);
    line.SetLineConfigCallback(nullptr);
    taps_[index]->ClearBreak();
}

bool SerialLineSwitch::Tap::Selected() const {
    return owner_.selected_index_.load(std::memory_order_acquire) == index_;
}

void SerialLineSwitch::Tap::OnGuestTx(const uint8_t* data, size_t n) { owner_.port_.DeliverTx(data, n, true); }

void SerialLineSwitch::Tap::OnControlLines(bool dtr, bool rts) {
    if (!Selected()) return;
    owner_.dtr_.store(dtr, std::memory_order_release);
    owner_.rts_.store(rts, std::memory_order_release);
    owner_.port_.NotifyControlLines();
}

void SerialLineSwitch::Tap::OnBreak(bool on) {
    in_break_.store(on, std::memory_order_release);
    owner_.port_.NotifyControlLines();
}

void SerialLineSwitch::Tap::ResendModemInputs() {
    if (Selected()) owner_.port_.ResendModemInputs();
}

}
