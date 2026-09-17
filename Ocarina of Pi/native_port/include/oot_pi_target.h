#ifndef OOT_PI_TARGET_H
#define OOT_PI_TARGET_H

#include <stdint.h>

/* N64-faithful native Raspberry Pi 2B target contract. */
enum {
    OOT_PI_FRAMEBUFFER_WIDTH = 320,
    OOT_PI_FRAMEBUFFER_HEIGHT = 240,
    OOT_PI_GAME_TICK_HZ = 20,
    OOT_PI_MAX_GAME_TICK_HZ = 30,
};

typedef struct OotPiFrameClock {
    uint64_t next_tick_ns;
    uint32_t tick_hz;
} OotPiFrameClock;

/* Returns zero when the requested cadence is outside the 1..30 Hz limit. */
int oot_pi_frame_clock_init(OotPiFrameClock* clock, uint32_t tick_hz);
void oot_pi_frame_clock_wait(OotPiFrameClock* clock);
uint64_t oot_pi_monotonic_time_ns(void);

#endif
