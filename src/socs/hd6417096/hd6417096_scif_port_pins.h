#pragma once

#include <cerfcore/core/callback_list.h>
#include <cerfcore/core/service.h>

#include <atomic>
#include <cstdint>

namespace cerfcore {

inline constexpr uint8_t kHd6417096ScifPortBits = 0xF0u;

class Hd6417096ScifPortPins : public Service {
public:
    using Service::Service;

    bool ShouldRegister() override;

    void    Store(uint8_t bits);
    void    Load(uint8_t bits);
    uint8_t Bits() const;
    bool    TxdPortLow() const;
    void    SetRtsFunctionHigh(bool high);
    bool    RtsHigh() const;
    void    AddChangeListener(CallbackList<>::Callback listener);

private:
    std::atomic<uint8_t> bits_{0u};
    std::atomic<bool>    function_high_{false};
    CallbackList<>       listeners_;
};

}
