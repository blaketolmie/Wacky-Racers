#include "racer_heartbeat.h"
#include "pio.h"
#include "target.h"

#define HEARTBEAT_LED_PIO LED_STATUS_PIO
#define HEARTBEAT_TICKS 50

void racer_heartbeat_init(void)
{
    pio_config_set(HEARTBEAT_LED_PIO, PIO_OUTPUT_HIGH);
}

void racer_heartbeat_update(void)
{
    static uint8_t ticks = 0;

    ticks++;
    if (ticks >= HEARTBEAT_TICKS)
    {
        ticks = 0;
        pio_output_toggle(HEARTBEAT_LED_PIO);
    }
}
