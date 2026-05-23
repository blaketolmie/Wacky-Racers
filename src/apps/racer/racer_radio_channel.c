#include "racer_radio_channel.h"
#include "pio.h"
#include "target.h"

/*
   The DIP switches are read as a 4-bit number.
   DIP 1 is the low bit and DIP 4 is the high bit.
   The switches use pullups, so an ON/closed switch reads low.
*/
#define RADIO_BASE_CHANNEL 84

void racer_radio_channel_init(void)
{
    pio_config_set(DIP_SW_1_PIO, PIO_PULLUP);
    pio_config_set(DIP_SW_2_PIO, PIO_PULLUP);
    pio_config_set(DIP_SW_3_PIO, PIO_PULLUP);
    pio_config_set(DIP_SW_4_PIO, PIO_PULLUP);
}

uint8_t racer_radio_channel_get(void)
{
    uint8_t channel_offset = 0;

    if (! pio_input_get(DIP_SW_1_PIO))
        channel_offset |= 1;
    if (! pio_input_get(DIP_SW_2_PIO))
        channel_offset |= 2;
    if (! pio_input_get(DIP_SW_3_PIO))
        channel_offset |= 4;
    if (! pio_input_get(DIP_SW_4_PIO))
        channel_offset |= 8;

    return RADIO_BASE_CHANNEL + channel_offset;
}
