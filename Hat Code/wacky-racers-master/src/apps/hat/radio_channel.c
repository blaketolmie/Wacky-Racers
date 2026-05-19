#include "radio_channel.h"
#include "pio.h"
#include "target.h"

/*
   The DIP switches are read as a 4-bit number.
   DIP 1 is the low bit and DIP 4 is the high bit.
   The switches use pullups, so an ON/closed switch reads low.
*/
#define RADIO_BASE_CHANNEL 84

void radio_channel_init(void)
{
    pio_config_set(DIPSW_1_PIO, PIO_PULLUP);
    pio_config_set(DIPSW_2_PIO, PIO_PULLUP);
    pio_config_set(DIPSW_3_PIO, PIO_PULLUP);
    pio_config_set(DIPSW_4_PIO, PIO_PULLUP);
}

uint8_t radio_channel_get(void)
{
    uint8_t channel_offset = 0;

    if (! pio_input_get(DIPSW_1_PIO))
        channel_offset |= 1;
    if (! pio_input_get(DIPSW_2_PIO))
        channel_offset |= 2;
    if (! pio_input_get(DIPSW_3_PIO))
        channel_offset |= 4;
    if (! pio_input_get(DIPSW_4_PIO))
        channel_offset |= 8;

    return RADIO_BASE_CHANNEL + channel_offset;
}
