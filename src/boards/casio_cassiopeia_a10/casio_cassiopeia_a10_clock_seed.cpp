#include "../../socs/hd6417096/hd6417096_clock_seed.h"

#include <cerfcore/boards/board_context.h>
#include <cerfcore/core/cerf_emulator.h>
#include "../../peripherals/rtc/bcd_calendar.h"
#include "casio_cassiopeia_a10_family_id.h"

#include <cstdint>

namespace cerfcore {

namespace {

constexpr uint32_t kFirstYear      = 1900u;
constexpr uint32_t kFirstMonth     = 3u;
constexpr uint32_t kMonthsPerYear  = 12u;
constexpr uint64_t kSecondsPerDay  = 86400u;
constexpr uint64_t kSecondsPerHour = 3600u;
constexpr uint64_t kSecondsPerMin  = 60u;

class CasioCassiopeiaA10ClockSeed : public Hd6417096ClockSeed {
public:
    using Hd6417096ClockSeed::Hd6417096ClockSeed;

    bool ShouldRegister() override {
        return emu_.Get<BoardContext>().GetBoardFamilyId() == BoardFamilyId::CasioCassiopeiaA10;
    }

    uint64_t PowerOnSeconds() override {
        const BcdCalendar::HostDate host = BcdCalendar::HostNow();
        return DayNumber(host.year, host.month, host.date) * kSecondsPerDay + host.hour * kSecondsPerHour +
               host.min * kSecondsPerMin + host.sec;
    }

private:
    static uint64_t DayNumber(uint32_t year, uint32_t month, uint32_t date) {
        uint64_t days = date;
        uint32_t y    = kFirstYear;
        uint32_t m    = kFirstMonth;
        while (y < year || (y == year && m < month)) {
            days += BcdCalendar::DaysInMonth(m, y);
            if (++m > kMonthsPerYear) {
                m = 1u;
                ++y;
            }
        }
        return days;
    }
};

}

REGISTER_SERVICE_AS(CasioCassiopeiaA10ClockSeed, Hd6417096ClockSeed);

}
