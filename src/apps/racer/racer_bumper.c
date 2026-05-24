#include <stdio.h>

#include "racer_bumper.h"
#include "pio.h"
#include "target.h"

#define BUMPER_DISABLE_SECONDS 5
#define BUTTON_POLL_RATE 100
#define HBRIDGE_OFF_TICKS (BUMPER_DISABLE_SECONDS * BUTTON_POLL_RATE)

static const button_cfg_t bumper_button_cfg =
{
    .pio = BUMPER_PIO
};

int racer_bumper_init(racer_bumper_t *bumper)
{
    bumper->hbridge_off_ticks = 0;
    bumper->button = button_init(&bumper_button_cfg);
    if (! bumper->button)
        return 1;

    return 0;
}

void racer_bumper_reset(racer_bumper_t *bumper)
{
    bumper->hbridge_off_ticks = 0;
    pio_output_high(HBRIDGE_ENABLE_PIO);
}

bool racer_bumper_update(racer_bumper_t *bumper)
{
    bool pushed = false;

    button_poll(bumper->button);

    if (button_pushed_p(bumper->button))
    {
        pushed = true;
        bumper->hbridge_off_ticks = HBRIDGE_OFF_TICKS;
        pio_output_low(HBRIDGE_ENABLE_PIO);
        printf("Bumper pushed: H-bridge disabled for 5 seconds and STOP requested\r\n");
        fflush(stdout);
    }

    if (bumper->hbridge_off_ticks > 0)
    {
        bumper->hbridge_off_ticks--;
        if (bumper->hbridge_off_ticks == 0)
        {
            pio_output_high(HBRIDGE_ENABLE_PIO);
            printf("H-bridge enabled\r\n");
            fflush(stdout);
        }
    }

    return pushed;
}

bool racer_bumper_is_active(racer_bumper_t *bumper)
{
    return bumper->hbridge_off_ticks > 0;
}
