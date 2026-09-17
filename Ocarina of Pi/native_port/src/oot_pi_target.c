#include "oot_pi_target.h"

#include <time.h>

static uint64_t timespec_to_ns(const struct timespec* value) {
    return (uint64_t)value->tv_sec * UINT64_C(1000000000) + (uint64_t)value->tv_nsec;
}

uint64_t oot_pi_monotonic_time_ns(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return timespec_to_ns(&now);
}

int oot_pi_frame_clock_init(OotPiFrameClock* clock, uint32_t tick_hz) {
    if (clock == 0 || tick_hz == 0 || tick_hz > OOT_PI_MAX_GAME_TICK_HZ) {
        return 0;
    }
    clock->tick_hz = tick_hz;
    clock->next_tick_ns = oot_pi_monotonic_time_ns();
    return 1;
}

void oot_pi_frame_clock_wait(OotPiFrameClock* clock) {
    const uint64_t interval_ns = UINT64_C(1000000000) / clock->tick_hz;
    uint64_t now = oot_pi_monotonic_time_ns();
    if (now < clock->next_tick_ns) {
        uint64_t remaining = clock->next_tick_ns - now;
        struct timespec sleep_time = {
            .tv_sec = (time_t)(remaining / UINT64_C(1000000000)),
            .tv_nsec = (long)(remaining % UINT64_C(1000000000)),
        };
        nanosleep(&sleep_time, 0);
        now = oot_pi_monotonic_time_ns();
    }
    if (now > clock->next_tick_ns + interval_ns) {
        clock->next_tick_ns = now;
    }
    clock->next_tick_ns += interval_ns;
}
