#include "../../src/peripherals/usb/usb_hub.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/state/state_stream.h>

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

using namespace cerfcore;

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void CheckHubStatus() {
    UsbHub hub(2);
    Require(hub.BeginControlTransfer({0x00, 9, 1, 0, 0}), "configure hub");
    hub.FinishControlTransfer();
    Require(hub.BeginControlTransfer({0xA0, 0, 0, 0, 4}), "hub status request stalled");
    std::array<uint8_t, 8> bytes;
    bytes.fill(0xFF);
    Require(hub.ReadControlReply(bytes.data(), bytes.size()) == 4, "hub status reply length");
    Require(bytes == std::array<uint8_t, 8>{0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF},
        "powered hub without overcurrent reports no status changes");
    Require(hub.ReadControlReply(bytes.data(), bytes.size()) == 0, "hub status reply exhausted");
    hub.FinishControlTransfer();
    for (const auto setup : {UsbDevice::SetupPacket{0xA0, 0, 1, 0, 4},
            UsbDevice::SetupPacket{0xA0, 0, 0, 1, 4},
            UsbDevice::SetupPacket{0xA0, 0, 0, 0, 2},
            UsbDevice::SetupPacket{0x20, 0, 0, 0, 4}})
        Require(!hub.BeginControlTransfer(setup), "malformed hub status must stall");
}

uint8_t DeviceStatus(UsbDevice& device) {
    Require(device.BeginControlTransfer({0x80, 0, 0, 0, 2}), "device status request");
    std::array<uint8_t, 2> status{};
    Require(device.ReadControlReply(status.data(), status.size()) == 2, "device status length");
    Require(status[1] == 0, "device status reserved bits");
    return status[0];
}

void Configure(UsbDevice& device, uint8_t value = 1) {
    Require(device.BeginControlTransfer({0x00, 9, value, 0, 0}), "configure device");
}

void EnableWake(UsbDevice& device, bool enabled = true) {
    Require(device.BeginControlTransfer({0x00, static_cast<uint8_t>(enabled ? 3 : 1), 1, 0, 0}),
        "set remote wakeup feature");
}

void CheckRemoteWakeup() {
    UsbHub hub(2);
    unsigned wakes = 0;
    hub.SetResumeHandler([&] { ++wakes; });
    Require(!hub.BeginControlTransfer({0, 3, 1, 0, 0}), "unconfigured device cannot arm wakeup");
    Configure(hub);
    Require(DeviceStatus(hub) == 1, "self-powered hub defaults to wake disabled");
    hub.SetSuspended(true);
    hub.Port(1).Attach(std::make_unique<UsbHub>(1));
    Require(wakes == 0, "disabled wakeup cannot resume on insertion");
    EnableWake(hub);
    Require(DeviceStatus(hub) == 3, "GET_STATUS exposes armed remote wakeup");
    hub.Port(1).Detach();
    Require(wakes == 1, "removal wakes armed suspended hub");
    hub.SetSuspended(false);
    hub.Port(1).Attach(std::make_unique<UsbHub>(1));
    Require(wakes == 1, "running hub needs no resume");
    hub.SetSuspended(true);
    EnableWake(hub, false);
    hub.Port(1).Detach();
    Require(wakes == 1 && DeviceStatus(hub) == 1, "CLEAR_FEATURE disarms wakeup");
    EnableWake(hub);
    hub.ResetToDefault();
    Require(DeviceStatus(hub) == 1, "bus reset disarms wakeup");
    hub.Port(1).Attach(std::make_unique<UsbHub>(1));
    Require(wakes == 1, "reset prevents stale resume");
}

void CheckWakeRestore() {
    CerfEmulator emu{{}, 0, nullptr};
    const char* path = "usb-hub-wakeup-state.bin";
    UsbHub source(2);
    Configure(source);
    EnableWake(source);
    source.SetSuspended(true);
    {
        StateWriter writer(emu, path);
        source.SaveState(writer);
        Require(writer.Commit(), "save armed suspended hub");
    }
    UsbHub restored(2);
    unsigned wakes = 0;
    restored.SetResumeHandler([&] { ++wakes; });
    {
        StateReader reader(emu, path);
        restored.RestoreState(reader);
        Require(reader.Ok(), "restore armed suspended hub");
    }
    Require(DeviceStatus(restored) == 3, "remote wakeup survives restore");
    restored.Port(1).Attach(std::make_unique<UsbHub>(1));
    Require(wakes == 1, "restored suspended hub wakes on insertion");
    std::remove(path);
}
}

int main() {
    try {
        CheckHubStatus();
        CheckRemoteWakeup();
        CheckWakeRestore();
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
