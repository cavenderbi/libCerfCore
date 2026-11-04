#pragma once

#include <cstdint>

namespace cerfcore {

class GpeLineWalk {
public:
    GpeLineWalk(int32_t x, int32_t y, uint32_t dir, int64_t d_m, int64_t d_n, int64_t gamma)
        : step_(kDir[dir & 7u]), x_(x), y_(y), accum_(d_n + gamma), axial_(d_n),
          diagonal_(d_n - d_m) {}

    int32_t X() const { return x_; }
    int32_t Y() const { return y_; }

    void Advance() {
        x_ += step_[0];
        y_ += step_[1];
        if (axial_ == 0) return;
        if (accum_ < 0) {
            accum_ += axial_;
            return;
        }
        x_ += step_[2];
        y_ += step_[3];
        accum_ += diagonal_;
    }

private:
    static constexpr int8_t kDir[8][4] = {
        {  1,  0,  0,  1 }, {  0,  1,  1,  0 }, {  0,  1, -1,  0 }, { -1,  0,  0,  1 },
        { -1,  0,  0, -1 }, {  0, -1, -1,  0 }, {  0, -1,  1,  0 }, {  1,  0,  0, -1 },
    };

    const int8_t* step_;
    int32_t       x_;
    int32_t       y_;
    int64_t       accum_;
    int64_t       axial_;
    int64_t       diagonal_;
};

}
