#include "../../src/host/paced_wave_out.h"
#include "../../src/platform/wave_out_backend.h"
#include <cerfcore/core/cerf_emulator.h>
#include <cerfcore/core/fatal.h>
#include <cerfcore/core/instance_threads.h>
#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <deque>
#include <stdexcept>
#include <string>

namespace {
cerfcore::WaveOutSink::DoneHandler done;
cerfcore::WaveOutSink::CommandHandler command;
std::deque<uint32_t> slots, commands;
std::string logs;
void Check(bool ok) { if (!ok) throw std::runtime_error("audio pacing check failed"); }
void Pump() { while (!commands.empty()) { auto c = commands.front(); commands.pop_front(); command(c); } }
void Complete() { Check(!slots.empty()); auto s = slots.front(); slots.pop_front(); done(s); Pump(); }
}

namespace cerfcore {
CerfEmulator::CerfEmulator(const CerfConfig& config, int argc, char** argv)
    : config_(config), argc_(argc), argv_(argv) {}
CerfEmulator::~CerfEmulator() { TearDown(); }
void CerfEmulator::OnServiceDestroy(const servicelocator::Service&) {}
void CerfEmulator::ShutDownService(servicelocator::Service& service) { service.OnShutdown(); }
void CerfEmulator::RaiseLocatorError(const std::string& message) { throw std::runtime_error(message); }
Fatal::Fatal(CerfEmulator& emu) : Service(emu) {}
void Fatal::OnReady() {}
void Fatal::Die(const char*, ...) { throw std::runtime_error("unexpected fatal"); }
void InstanceThreads::OnReady() {}
namespace Log {
namespace detail { std::atomic<uint64_t> enabled_mask{MASK_ALL}; }
void Print(Cat, const char* format, ...) {
    char text[1024]; va_list args; va_start(args, format);
    std::vsnprintf(text, sizeof(text), format, args); va_end(args); logs += text;
}
}
WaveOutSink::WaveOutSink() = default;
WaveOutSink::~WaveOutSink() = default;
void WaveOutSink::Start(Fatal&, InstanceThreads&, StartHandler start, CommandHandler c,
                        DoneHandler d, const char*) { command = c; done = d; start(); }
void WaveOutSink::Stop() {}
void WaveOutSink::PostCommand(uint32_t c) { commands.push_back(c); }
bool WaveOutSink::EnsureFormat(uint32_t, uint16_t, uint16_t, bool) { return true; }
bool WaveOutSink::IsOpen() const { return true; }
bool WaveOutSink::SamplePosition(uint32_t&, uint32_t& generation) const { generation = 1u; return false; }
void WaveOutSink::Play(uint32_t slot, const uint8_t*, uint32_t) { slots.push_back(slot); }
void WaveOutSink::Reset() { while (!slots.empty()) { done(slots.front()); slots.pop_front(); } }
}

int main() {
    cerfcore::DynamicRateResampler rate;
    cerfcore::DevicePosition p{true, 0u, 1.0, 1u, 0u, 48000u};
    rate.Sample(p);
    p.samples = 48048u; p.wall_s = 2.0;
    rate.Sample(p);
    double ratio = 0.0;
    Check(rate.TakeEstimate(&ratio) && std::abs(ratio - 1.001) < 0.000001);
    ++p.epoch; p.samples += 24000u; p.wall_s = 3.0;
    rate.Sample(p);
    Check(rate.TakeEstimate(&ratio) && std::abs(ratio - 1.001) < 0.000001);
    p.samples += 48048u; p.wall_s = 4.0;
    rate.Sample(p);
    Check(rate.TakeEstimate(&ratio) && std::abs(ratio - 1.001) < 0.000001);
    cerfcore::CerfConfig config;
    cerfcore::CerfEmulator emu(config, 0, nullptr);
    cerfcore::Fatal fatal(emu);
    cerfcore::InstanceThreads threads(emu);
    cerfcore::PacedWaveOut out;
    out.Start(fatal, threads, "test", 48000u, 2u, 16u);
    out.BeginAudioOut();
    uint8_t pcm[1920] = {};
    for (uint32_t i = 0u; i < 40u; ++i) { Check(out.QueueOutput(pcm, sizeof(pcm))); Pump(); }
    for (uint32_t i = 0u; i < 32u; ++i) Complete();
    Check(slots.size() == 8u);
    logs.clear();
    for (uint32_t i = 0u; i < 200u; ++i) { Complete(); Check(out.QueueOutput(pcm, sizeof(pcm))); Pump(); }
    Check(logs.find("unfed 0") != std::string::npos);
    Check(logs.find("starved 0") != std::string::npos);
    size_t at = 0u;
    while ((at = logs.find("unfed ", at)) != std::string::npos) {
        Check(logs.substr(at, 7u) == "unfed 0"); at += 7u;
    }
    while (!slots.empty()) Complete();
    logs.clear();
    for (uint32_t i = 0u; i < 130u; ++i) {
        Check(out.QueueOutput(pcm, sizeof(pcm))); Pump();
        if (slots.size() > 1u) Complete();
    }
    Check(logs.find("unfed 0") == std::string::npos);
    Check(logs.find("unfed ") != std::string::npos);
    std::puts("audio_pacing_test passed");
}
