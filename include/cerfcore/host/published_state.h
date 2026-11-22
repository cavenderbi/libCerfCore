#pragma once

#include <mutex>
#include <optional>
#include <utility>

namespace cerfcore {

template <class State>
class PublishedState {
public:
    template <class Read>
    bool Replace(Read read) {
        return Replace(read, [](const State&) {});
    }

    template <class Read, class Changed>
    bool Replace(Read read, Changed changed) {
        std::lock_guard<std::mutex> lk(mtx_);
        State now = read();
        if (published_ && *published_ == now) return false;
        published_ = std::move(now);
        changed(*published_);
        return true;
    }

private:
    std::mutex           mtx_;
    std::optional<State> published_;
};

}
